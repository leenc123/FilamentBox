// filament_box.ino — FilamentBox 主程序（ESP32 + 4xRC522 → 拓竹 AMS Lite）
// 轮询 4 路读卡 → 解码 → 查表 → 去抖 → 变化才 TLS 短连推送
// ams_filament_setting → OLED 同步显示；无网络自动开 AP 配网。
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "filament_map.h"
#include "rfid_store.h"
#include "rfid_reader.h"
#include "mqtt_push.h"
#include "oled_ui.h"
#include "write_mode.h"
#include "wifi_portal.h"
#include "printer_setup.h"
#include "web_dash.h"
#include "status_led.h"

#define WIFI_DOWN_REBOOT_MS 120000  // 运行中掉线超此时长没连上则重启（重启后自动进配网）

static RfidReader reader(SPI_SCK_PIN, SPI_MOSI_PIN, SPI_MISO_PIN, RC522_RST_PIN,
                         SLOT_CS, NUM_SLOTS);
static OledUi ui(OLED_SDA_PIN, OLED_SCL_PIN);

static String slotKey[NUM_SLOTS];   // "TYPE|COLOR"，空字符串 = 无卡/非法
static String slotSent[NUM_SLOTS];  // 已推送过的 key，防重复
static uint8_t stableCount[NUM_SLOTS] = {0};
static unsigned long seqId = 2001;
static String statusText = "boot";
static String shownKey = "";
static unsigned long wifiDownSince = 0;
static PrinterCfg gPrinter;          // 运行时打印机配置（NVS 优先，config.h 保底）
static uint8_t pushFailStreak = 0;   // 连续推送失败计数
static bool printerHintShown = false;

static void refreshOled() {
  if (!ui.ok()) return;
  // 提示 sticky：连续失败达阈值且 WiFi 正常时，始终显示配置提示屏
  if (printerHintShown && pushFailStreak >= PRINTER_HINT_STREAK &&
      WiFi.status() == WL_CONNECTED) {
    showPrinterHint(&ui);
    return;
  }
  String types[NUM_SLOTS], colors[NUM_SLOTS];
  uint8_t trays[NUM_SLOTS];
  String key = statusText + "|";
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    trays[i] = trayDisplayNo(i);  // OLED 显示 1-4
    int sep = slotKey[i].indexOf('|');
    if (sep < 0) {
      types[i] = "";
      colors[i] = "";
    } else {
      String key = slotKey[i].substring(0, sep);
      colors[i] = slotKey[i].substring(sep + 1);
      const FilamentInfo* info = lookupFilament(key);
      // OLED 显示短名（去厂商前缀），超 12 字符截断以保住颜色列
      String shown = info ? String(shortFilamentName(info)) : key;
      if (shown.length() > 12) shown = shown.substring(0, 12);
      types[i] = shown;
    }
    key += slotKey[i] + ";";
  }
  if (key == shownKey) return;  // 内容不变不重刷（省 I2C、防闪烁）
  shownKey = key;
  ui.showSlots(types, trays, colors, statusText);
}

// 对 i 槽按当前 slotKey 组包并推送一次；同步 statusText / 失败计数 / OLED 提示标记
// loop 轮询与网页写卡后验证共用
static bool pushSlotNow(uint8_t i, String& errOut) {
  int sep = slotKey[i].indexOf('|');
  if (sep < 0) { errOut = "empty slot"; return false; }
  String t = slotKey[i].substring(0, sep);
  String c = slotKey[i].substring(sep + 1);
  const FilamentInfo* info = lookupFilament(t);
  if (!info) { errOut = "unknown type"; return false; }
  String payload = buildAmsSetting(seqId, AMS_ID, SLOT_TRAY[i], *info, c, info->type);
  String err;
  if (pushAmsSetting(gPrinter.ip.c_str(), gPrinter.serial.c_str(),
                     gPrinter.code.c_str(), payload, gPrinter.mqttPort(),
                     !gPrinter.dbg, &err)) {
    seqId++;
    pushFailStreak = 0;
    if (printerHintShown) {
      // 恢复：清提示标记并强制重刷槽位屏
      printerHintShown = false;
      shownKey = "";
    }
    ledSuccessPulse();  // SENT 脉冲：快闪 3 下后回心跳
    String shown = String(shortFilamentName(info));
    if (shown.length() > 12) shown = shown.substring(0, 12);
    statusText = "SENT " + String(trayDisplayNo(i)) + " " + shown;
    Serial.println(statusText);
    return true;
  }
  pushFailStreak++;
  statusText = "FAIL " + String(trayDisplayNo(i));
  Serial.println(err);
  errOut = err;
  // WiFi 通但连续失败达阈值：弹 OLED 配置提示（refreshOled 内 sticky 显示）
  if (pushFailStreak >= PRINTER_HINT_STREAK &&
      WiFi.status() == WL_CONNECTED) {
    printerHintShown = true;
    ledSet(LED_ERROR);  // 告警快闪，直到推送成功恢复
  }
  return false;
}

// ---- 网页后台（管理首页 / + 写卡 /write）----

static void snapshotSlots(DashSlot out[NUM_SLOTS]) {
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    int sep = slotKey[i].indexOf('|');
    if (sep < 0) {
      out[i].type = "";
      out[i].color = "";
    } else {
      // 首页显示全名（如 Bambu PLA Matte），旧卡显示原字符串
      String key = slotKey[i].substring(0, sep);
      const FilamentInfo* info = lookupFilament(key);
      out[i].type = info ? String(info->name) : key;
      out[i].color = slotKey[i].substring(sep + 1, sep + 7);  // RRGGBB
    }
  }
}

static void handleDashRoot() {
  DashSlot s[NUM_SLOTS];
  snapshotSlots(s);
  setupWebServer().send(200, "text/html; charset=utf-8", dashHome(s, statusText));
}

static void handleDashWriteForm() {
  setupWebServer().send(200, "text/html; charset=utf-8", dashWriteForm("", true));
}

static void handleDashWrite() {
  WebServer& web = setupWebServer();
  if (!web.hasArg("slot") || !web.hasArg("type") || !web.hasArg("color")) {
    web.send(400, "text/html; charset=utf-8", dashWriteForm("bad args", false));
    return;
  }
  int slot = web.arg("slot").toInt();
  String type = web.arg("type");
  String color = web.arg("color");
  color.toUpperCase();
  type.trim();
  color.trim();

  if (slot < 0 || slot >= NUM_SLOTS) {
    web.send(400, "text/html; charset=utf-8", dashWriteForm("bad slot", false));
    return;
  }
  const FilamentInfo* winfo = lookupFilament(type);
  if (!winfo) {
    web.send(400, "text/html; charset=utf-8", dashWriteForm("unknown type", false));
    return;
  }
  if (!isPaletteColor(color)) {
    web.send(400, "text/html; charset=utf-8",
             dashWriteForm("bad color: use palette", false));
    return;
  }
  uint8_t b4[16], b5[16];
  if (!encodeCard(type, color, b4, b5)) {
    web.send(400, "text/html; charset=utf-8", dashWriteForm("encode fail", false));
    return;
  }
  if (!reader.writeSlot((uint8_t)slot, b4, b5)) {
    web.send(500, "text/html; charset=utf-8",
             dashWriteResult(false, "write failed: 卡是否放在该槽读卡器上?"));
    return;
  }
  // 写成功：同步槽位快照并立即推送到对应 AMS 槽验证；
  // slotSent 先留空：推送失败时主循环按正常流程自动重试
  slotKey[slot] = type + "|" + color;
  stableCount[slot] = STABLE_COUNT;
  slotSent[slot] = "";
  String err;
  if (pushSlotNow((uint8_t)slot, err)) {
    slotSent[slot] = slotKey[slot];
    web.send(200, "text/html; charset=utf-8",
             dashWriteResult(true, "OK 槽位 " + String(trayDisplayNo((uint8_t)slot)) +
                             " " + String(winfo->name) + "（" + statusText + "）"));
  } else {
    web.send(200, "text/html; charset=utf-8",
             dashWriteResult(false, "卡已写入但推送失败（" + statusText +
                             "），设备稍后会自动重试"));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] start");
  reader.begin();
  Serial.println("[BOOT] reader ok");
  ledBegin();  // GPIO2 strapping 安全点：reader 之后、按键判断之前
  ledSet(LED_WIFI_CONNECT);

  // 启动写卡模式：按住按键超 3 秒进 AP（阻塞，不再往下走）
  if (shouldEnterWriteMode(BUTTON_PIN, BUTTON_HOLD_MS)) {
    Serial.println("[BOOT] write mode");
    runWriteMode(reader);
    return;
  }

  // OLED 先初始化，后续 WiFi 连接/配网状态全程显示在屏上
  ui.begin();

  // 打印机配置：NVS 优先，config.h 保底
  loadPrinterCfg(gPrinter);
  Serial.println("[BOOT] printer cfg ok");

  // 无网络自动开 AP 配网（阻塞直到配网成功重启；OLED 显示进度）
  ensureWifiOrPortal(&ui);
  Serial.println("[BOOT] wifi ok");
  ledSet(LED_HEARTBEAT);  // 联网成功回心跳

  // 开机 IP 页：连上 WiFi 先显示本机 IP，停几秒再切主页
  if (ui.ok()) {
    String splash[5] = {
      "WiFi OK",
      WiFi.localIP().toString(),
      "/setup",
      "to homepage...",
      ""
    };
    ui.showLines(splash, 5);
    Serial.print("[BOOT] ip ");
    Serial.println(WiFi.localIP());
    for (uint16_t w = 0; w < OLED_IP_SPLASH_MS; w += 50) { ledTick(); delay(50); }
    shownKey = "";  // 强制重刷主页
  }

  // WiFi 连上之后再起常驻 Web 服务（管理首页 / + 写卡 /write + 配置 /setup）
  startPrinterSetup(gPrinter);
  WebServer& web = setupWebServer();
  web.on("/", handleDashRoot);
  web.on("/write", HTTP_GET, handleDashWriteForm);
  web.on("/write", HTTP_POST, handleDashWrite);
  Serial.println("[BOOT] setup server ok");

  statusText = "idle";
  refreshOled();
  Serial.println("FilamentBox ready");
}

void loop() {
  ledTick();
  // 运行中掉线：OLED 显示重连状态；超 2 分钟连不上则重启（重启后自动进配网）
  if (WiFi.status() != WL_CONNECTED) {
    ledSet(LED_WIFI_CONNECT);
    ledTick();
    if (wifiDownSince == 0) {
      wifiDownSince = millis();
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin();  // 用上次凭据（NVS/默认）重连
    }
    statusText = "WiFi retry";
    refreshOled();
    if (millis() - wifiDownSince > WIFI_DOWN_REBOOT_MS) ESP.restart();
    for (uint16_t w = 0; w < POLL_MS; w += 50) { ledTick(); delay(50); }
    return;
  }
  if (wifiDownSince != 0) {
    // 刚恢复连接：状态回 idle 并强制重刷一屏
    wifiDownSince = 0;
    statusText = "idle";
    shownKey = "";
    if (!printerHintShown) ledSet(LED_HEARTBEAT);
  }
  printerSetupHandle();  // 常驻 /setup 配置页请求处理

  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    uint8_t b4[16], b5[16];
    String key = "";
    if (reader.readSlot(i, b4, b5)) {
      String t, c;
      if (decodeCard(b4, b5, t, c) && checkColor(c) && lookupFilament(t)) {
        key = t + "|" + c;
      }
    }
    if (key == slotKey[i]) {
      if (stableCount[i] < 255) stableCount[i]++;
    } else {
      stableCount[i] = 0;
      slotKey[i] = key;
    }

    if (stableCount[i] >= STABLE_COUNT && key.length() > 0 && key != slotSent[i]) {
      String err;
      pushSlotNow(i, err);
      slotSent[i] = key;
    }
  }

  refreshOled();
  for (uint16_t w = 0; w < POLL_MS; w += 50) { ledTick(); delay(50); }
}
