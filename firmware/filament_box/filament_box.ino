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
#include "wifi_portal.h"
#include "printer_setup.h"
#include "log_ring.h"
#include "web_dash.h"
#include "status_led.h"
#include "miaoui_menu.h"

#define WIFI_DOWN_REBOOT_MS 120000  // 运行中掉线超此时长没连上则重启（重启后自动进配网）

static RfidReader reader(SPI_SCK_PIN, SPI_MOSI_PIN, SPI_MISO_PIN, RC522_RST_PIN,
                         SLOT_CS, NUM_SLOTS);
static OledUi ui(OLED_SDA_PIN, OLED_SCL_PIN);

static String slotKey[NUM_SLOTS];   // "TYPE|COLOR"，空字符串 = 无卡/非法
static String slotSent[NUM_SLOTS];  // 已推送过的 key，防重复（失败也记：抑制自动重试，拔卡清标记后重插可再推）
static bool pushPending[NUM_SLOTS] = {false};  // 脏标记：轮询/写卡只置脏，loop 每轮最多推一槽
static uint8_t pushCursor = 0;      // 单槽推送轮转起点，避免固定槽位饿死
static uint8_t stableCount[NUM_SLOTS] = {0};
static unsigned long seqId = 2001;
static String statusText = "boot";
static String shownKey = "";
static unsigned long wifiDownSince = 0;
static PrinterCfg gPrinter;          // 运行时打印机配置（NVS 优先，config.h 保底）
static uint8_t pushFailStreak = 0;   // 连续推送失败计数
static bool printerHintShown = false;

// MiaoUI 喂数：把槽位快照拼成与老屏同格式的行（短名12宽+RRGGBB），状态行 idle 显示
// FBX Ready；打印机配置提示压缩成 "SETUP <ip>"（23字符缓冲放得下）。
static void feedMiaoui(const String& statusOverride = "") {
  if (!miaouiActive()) return;
  String types[NUM_SLOTS], colors[NUM_SLOTS];
  uint8_t trays[NUM_SLOTS], present[NUM_SLOTS];
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    trays[i] = trayDisplayNo(i);
    present[i] = reader.present(i) ? 1 : 0;  // 缓存标志，缺席2秒节流，开销可忽略
    int sep = slotKey[i].indexOf('|');
    if (sep < 0) {
      types[i] = "";
      colors[i] = "";
    } else {
      String key = slotKey[i].substring(0, sep);
      colors[i] = slotKey[i].substring(sep + 1);
      const FilamentInfo* info = lookupFilament(key);
      String shown = info ? String(shortFilamentName(info)) : key;
      if (shown.length() > 12) shown = shown.substring(0, 12);
      types[i] = shown;
    }
  }
  String st = statusOverride.length() ? statusOverride : statusText;
  if (printerHintShown && pushFailStreak >= PRINTER_HINT_STREAK &&
      WiFi.status() == WL_CONNECTED) {
    st = "SETUP " + WiFi.localIP().toString();
  }
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
  miaouiFeedAndTick(types, trays, colors, present, st, ip);
}

// 等待间隙跑 MiaoUI 动画+按键（~30ms 一步，无堆分配）；老屏回落时走 delay。
// 步进内高频处理 Web 请求：推送阻塞解除后、等待窗口期 Web 也能秒回。
static void waitPollWindow() {
  if (miaouiActive()) {
    unsigned long t0 = millis();
    while (millis() - t0 < POLL_MS) { ledTick(); miaouiTick(); printerSetupHandle(); delay(30); }
  } else {
    for (uint16_t w = 0; w < POLL_MS; w += 50) { ledTick(); printerSetupHandle(); delay(50); }
  }
}

static void refreshOled() {
  if (!ui.ok()) return;
  if (miaouiActive()) { feedMiaoui(); return; }
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
      // 空槽区分读卡器缺席（老屏 6x10 无中文，用英文标记；MiaoUI 屏显示"空 !"）
      if (reader.present(i)) {
        types[i] = "";
      } else {
        types[i] = "no reader";
      }
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
    key += reader.present(i) ? "1" : "0";  // 读卡器插拔也触发重刷
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
    logLine(statusText);
    return true;
  }
  pushFailStreak++;
  statusText = "FAIL " + String(trayDisplayNo(i));
  logLine("FAIL " + String(trayDisplayNo(i)) + " " + err);
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
  logLine("[WRITE] slot " + String(slot) + " type " + type + " color " + color);
  if (!reader.writeSlot((uint8_t)slot, b4, b5)) {
    logLine("[WRITE] slot " + String(slot) + " failed");
    web.send(500, "text/html; charset=utf-8",
             dashWriteResult(false, "write failed: 卡是否放在该槽读卡器上?"));
    return;
  }
  logLine("[WRITE] slot " + String(slot) + " OK");
  // 写成功：同步槽位快照并置脏，推送统一走 loop 后台（每轮最多一槽）；
  // 本回调内不再做 TLS，Web 秒回，避免浏览器超时重试导致重复写卡。
  // slotSent 不动：后台推成功才记；失败也记（抑制自动重试，拔卡清标记后重插可再推）。
  slotKey[slot] = type + "|" + color;
  stableCount[slot] = STABLE_COUNT;
  pushPending[slot] = true;
  statusText = "WRITE " + String(trayDisplayNo((uint8_t)slot)) + " pushing";
  shownKey = "";  // 强制刷屏，OLED/管理首页即时可见新卡
  web.send(200, "text/html; charset=utf-8",
           dashWriteResult(true, "OK 槽位 " + String(trayDisplayNo((uint8_t)slot)) +
                           " " + String(winfo->name) + "（已写入，推送中）"));
}

static void handleDashLog() {
  setupWebServer().send(200, "text/html; charset=utf-8",
                        dashLogPage(statusText, logSeq()));
}

// 增量轮询：首行 CURSOR <seq>，之后为 cursor 之后的新行；短请求即关，不占 loop
static void handleDashLogStream() {
  WebServer& web = setupWebServer();
  uint32_t cursor = 0;
  if (web.hasArg("cursor")) cursor = (uint32_t)web.arg("cursor").toInt();
  String t = "CURSOR " + String(logSeq()) + "\n";
  for (uint8_t i = 0; i < logCount(); i++) {
    if (logSeqAt(i) > cursor) t += logAt(i) + "\n";
  }
  web.send(200, "text/plain; charset=utf-8", t);
}

static void handleDashLogText() {
  setupWebServer().send(200, "text/plain; charset=utf-8", dashLogText());
}

static void handleDashLogClear() {
  logClear();
  logLine("[WEB] log cleared");
  WebServer& web = setupWebServer();
  web.sendHeader("Location", "/log");
  web.send(303, "text/plain", "");
}

// 屏上 Write 菜单执行体：材料×品牌解代表预设 + 12常用色解色值 → 编码 → 写卡 → 置脏后台推送。
// 与网页 /write 同一套校验 + 写入逻辑（参数合法性/读卡器在位/卡是否放好），
// 结果进 statusText（Slots 标题条 + 串口同步），调用方（Fbx_WriteCard）无需处理返回。
// 本函数内只做写卡（约 1 秒），不再做 TLS：推送统一走 loop 后台，避免占住 ui_loop
// tick 导致按键/动画卡死，也避免与轮询推送背靠背打打印机 8883。
bool fbxScreenWrite(int slot1, int matIdx, int brandIdx, int colorIdx) {
  String st = "WRITE bad args";
  bool ok = false;
  if (slot1 >= 1 && slot1 <= NUM_SLOTS &&
      matIdx >= 0 && (size_t)matIdx < SCREEN_MAT_COUNT &&
      (brandIdx == 0 || brandIdx == 1) &&
      colorIdx >= 0 && (size_t)colorIdx < SCREEN_COLOR_COUNT) {
    uint8_t slot = (uint8_t)(slot1 - 1);
    if (!reader.present(slot)) {
      st = "WRITE " + String(trayDisplayNo(slot)) + " no reader";
    } else {
      String type = String((brandIdx == 0)
          ? SCREEN_MATS[matIdx].bambu
          : SCREEN_MATS[matIdx].generic);
      uint8_t ci = SCREEN_COLORS[colorIdx];
      String color = (ci < FILAMENT_COLOR_COUNT)
          ? String(FILAMENT_COLORS[ci].rgb)
          : String(FILAMENT_COLORS[0].rgb);
      color.toUpperCase();
      const FilamentInfo* winfo = lookupFilament(type);
      uint8_t b4[16], b5[16];
      if (!winfo || !encodeCard(type, color, b4, b5)) {
        st = "WRITE " + String(trayDisplayNo(slot)) + " encode fail";
      } else {
        logLine("[WRITE] screen slot " + String(slot) + " type " + type + " color " + color);
        if (!reader.writeSlot(slot, b4, b5)) {
          st = "WRITE " + String(trayDisplayNo(slot)) + " no card";
        } else {
          slotKey[slot] = type + "|" + color;
          stableCount[slot] = STABLE_COUNT;
          pushPending[slot] = true;  // 后台 loop 推，slotSent 留给后台记
          st = "WRITE " + String(trayDisplayNo(slot)) + " pushing";
          ok = true;  // 卡已写上，推送中也算写卡成功
        }
      }
    }
  }
  statusText = st;
  shownKey = "";  // 强制刷屏（老屏直刷；MiaoUI 状态行下轮喂数同步）
  logLine(st);
  return ok;
}

// 屏上 Write 结果查询：供 Fbx_WriteCard 弹结果提示（返回 statusText 快照，调用方立即拷贝）
const char* fbxLastWriteMsg() { return statusText.c_str(); }

void setup() {
  Serial.begin(115200);
  logLine("[BOOT] start");
  reader.begin();
  logLine("[BOOT] reader ok");
  ledBegin();  // GPIO2 strapping 安全点：reader 之后再 ledBegin 即安全
  ledSet(LED_WIFI_CONNECT);

  // OLED 先初始化，后续 WiFi 连接/配网状态全程显示在屏上
  ui.begin();
  // MiaoUI 接管正常态显示；失败/无屏回落老直刷屏（配网阻塞段始终用老屏）
  if (ui.ok()) {
    bool miaoUp = miaouiSetupMenu();
    logLine("[MIAOUI] " + String(miaoUp ? "active" : "fallback") +
            " free=" + String((unsigned)ESP.getFreeHeap()));
  }

  // 打印机配置：NVS 优先，config.h 保底
  loadPrinterCfg(gPrinter);
  logLine("[BOOT] printer cfg ok");

  // 无网络自动开 AP 配网（阻塞直到配网成功重启；OLED 显示进度）
  ensureWifiOrPortal(&ui);
  logLine("[BOOT] wifi ok");
  ledSet(LED_HEARTBEAT);  // 联网成功回心跳

  // 开机 IP 页（仅老屏回落时显示；MiaoUI 接管时 IP 在 System>IP 菜单里看）
  if (ui.ok() && !miaouiActive()) {
    String splash[5] = {
      "WiFi OK",
      WiFi.localIP().toString(),
      "/setup",
      "to homepage...",
      ""
    };
    ui.showLines(splash, 5);
    logLine("[BOOT] ip " + WiFi.localIP().toString());
    for (uint16_t w = 0; w < OLED_IP_SPLASH_MS; w += 50) { ledTick(); delay(50); }
    shownKey = "";  // 强制重刷主页
  } else if (ui.ok()) {
    logLine("[BOOT] ip " + WiFi.localIP().toString());
  }

  // WiFi 连上之后再起常驻 Web 服务（管理首页 / + 写卡 /write + 配置 /setup）
  startPrinterSetup(gPrinter);
  WebServer& web = setupWebServer();
  web.on("/", handleDashRoot);
  web.on("/write", HTTP_GET, handleDashWriteForm);
  web.on("/write", HTTP_POST, handleDashWrite);
  web.on("/log", handleDashLog);
  web.on("/log/stream", handleDashLogStream);
  web.on("/log.txt", handleDashLogText);
  web.on("/log/clear", HTTP_POST, handleDashLogClear);
  logLine("[BOOT] setup server ok");

  statusText = "idle";
  refreshOled();
  logLine("FilamentBox ready");
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
    waitPollWindow();
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

  // 轮询只置脏不推送：同槽连续换卡时 slotKey 天然只留最新，旧值直接被覆盖丢弃。
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
    if (key.length() == 0) {
      // 拔卡：清已发送标记 + 取消本槽待推。失败曾记 slotSent=key 抑制自动重试，
      // 此处清掉后，手动重插同卡时 key != slotSent 成立，可再推一次。
      slotSent[i] = "";
      pushPending[i] = false;
      continue;
    }
    if (stableCount[i] >= STABLE_COUNT && key != slotSent[i]) {
      pushPending[i] = true;
    }
    printerSetupHandle();  // 逐槽间隙处理 Web，4 槽轮询期请求也能响应
  }

  // 后台推送：每轮最多一槽（轮转防饿死）。推前用 slotKey 最新值组包，
  // 同槽在轮询期被覆盖的旧值不会再推；失败也记 slotSent=key（不自动重试）。
  for (uint8_t n = 0; n < NUM_SLOTS; n++) {
    uint8_t i = (uint8_t)((pushCursor + n) % NUM_SLOTS);
    if (!pushPending[i]) continue;
    pushCursor = (uint8_t)((i + 1) % NUM_SLOTS);
    pushPending[i] = false;
    if (slotKey[i].length() == 0) { slotSent[i] = ""; break; }  // 推前复核：卡已拔则丢弃
    printerSetupHandle();  // 推送前让 Web 先回一次
    String err;
    String key = slotKey[i];  // 快照：判脏用（单线程推送期无并发改写，纯防御）
    if (pushSlotNow(i, err)) {
      if (slotKey[i] == key) slotSent[i] = key;
      else slotSent[i] = slotKey[i];  // 推送期被写卡覆盖：旧结果丢弃，记最新为已发由下轮复核
      if (slotKey[i] != key) pushPending[i] = true;  // 覆盖发生时补推最新
    } else {
      if (slotKey[i] == key) slotSent[i] = key;  // 失败抑制自动重试，拔卡清标记后重插可再推
    }
    printerSetupHandle();  // 推送后立刻处理 Web
    break;  // 一轮一槽，剩余脏槽下轮继续
  }

  refreshOled();
  waitPollWindow();
}
