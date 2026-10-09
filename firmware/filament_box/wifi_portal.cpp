// wifi_portal.cpp — WiFi 配网（WiFiManager 非阻塞模式 + 自带 Captive Portal）
// 开机先用 NVS/默认凭据直连 15 秒；连不上则开配网 AP，手机会自动弹出配网页。
// 配网成功后凭据同步存一份到本项目 NVS（loop 重连与 OLED 显示用）。
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <U8g2lib.h>
#include "wifi_portal.h"
#include "config.h"
#include "oled_ui.h"
#include "status_led.h"
#include "src/miaoui/display/dispDriver.h"
#include "src/miaoui/widget/custom.h"

#define PORTAL_TIMEOUT_MS 15000  // 开机直连超时：超则自动进 AP 配网

void loadWifiCreds(String& ssid, String& pass) {
  Preferences p;
  if (p.begin("filamentbox", true)) {
    ssid = p.getString("ssid", WIFI_SSID);
    pass = p.getString("pass", WIFI_PASS);
    p.end();
  } else {
    ssid = WIFI_SSID;
    pass = WIFI_PASS;
  }
}

void saveWifiCreds(const String& ssid, const String& pass) {
  Preferences p;
  if (p.begin("filamentbox", false)) {
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
  }
}

// OLED 图形帧：左侧图标位 + 右侧标题 + 最多 3 行信息（中英混排经 Cn_DrawStr，
// 中文 12px、ASCII 同高，一行 13px 间距）；超屏自动跳过（等价死区裁剪）；
// fracBar 0-100 画底部确定性进度条，<0 不画。
static void portalFrame(OledUi* ui, uint8_t art, uint8_t stage, const char* title,
                        const String& l1, const String& l2, const String& l3 = "",
                        int16_t fracBar = -1) {
  if (!ui || !ui->ok()) return;
  uint8_t yOff = OLED_TOP_DEAD;
  Disp_ClearBuffer();
  uint8_t c = 1;
  Disp_SetDrawColor(&c);
  uint8_t titleX;
  uint8_t titleY;
  if (art == 0) {
    Portal_Art(yOff, stage);
    titleX = 54;
    titleY = (uint8_t)(yOff + 12);
  } else {
    Portal_Result(yOff, art == 1 ? 1 : 0);
    titleX = 38;
    titleY = (uint8_t)(yOff + 16);
  }
  Cn_DrawStr(titleX, titleY, title);
  const String ls[3] = {l1, l2, l3};
  for (uint8_t i = 0; i < 3; i++) {
    if (ls[i].length() == 0) continue;
    uint8_t y = (uint8_t)(yOff + 34 + i * 13);
    if (y > 62) continue;
    Cn_DrawStr(0, y, ls[i].c_str());
  }
  if (fracBar >= 0) {
    if (fracBar > 100) fracBar = 100;
    Disp_DrawFrame(0, 57, 128, 6);
    uint8_t w = (uint8_t)(124 * fracBar / 100);
    if (w > 0) Disp_DrawBox(2, 59, w, 2);
  }
  Disp_SendBuffer();
}

bool connectWifiWith(const String& ssid, const String& pass,
                     unsigned long timeoutMs, OledUi* ui) {
  WiFi.mode(WIFI_STA);
  Serial.print("[WIFI] begin ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  ledSet(LED_WIFI_CONNECT);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    ledTick();
    // 波纹随已等待比例 1->3 道点亮 + 底部确定性进度条 + 剩余秒数，等得有盼头
    unsigned long el = millis() - t0;
    uint8_t frac = el >= timeoutMs ? 100 : (uint8_t)(el * 100 / timeoutMs);
    uint8_t stage = (uint8_t)(1 + frac * 2 / 100);
    unsigned long sec = (timeoutMs > el) ? (timeoutMs - el) / 1000 : 0;
    portalFrame(ui, 0, stage, "正在连接", ssid,
                "剩余 " + String(sec) + "秒", "", (int16_t)frac);
    for (uint8_t w = 0; w < 10; w++) { ledTick(); delay(50); }
  }
  Serial.print("[WIFI] result ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
  return WiFi.status() == WL_CONNECTED;
}

static void runPortal(OledUi* ui) {
  Serial.println("[WIFI] start portal AP");
  ledSet(LED_AP_PORTAL);
  WiFiManager wm;
  // 配网 AP 与之前手写页同名同 IP，老文档不用改
  wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                         IPAddress(255, 255, 255, 0));
  wm.setConfigPortalBlocking(false);  // 非阻塞：OLED 刷新由我们自己的循环做
  wm.setConfigPortalTimeout(0);       // 0 = 不超时，一直等到配好（与旧行为一致）
  wm.setCaptivePortalEnable(true);  // 手机连 AP 自动弹出配网页
  wm.setAPCallback([ui](WiFiManager*) {
    Serial.println("[WIFI] portal AP up");
    portalFrame(ui, 0, 2, "配网模式", "AP:" + String(SETUP_AP_SSID),
                "IP 192.168.4.1", "自动弹出配网页面", -1);
  });

  bool already = wm.autoConnect(SETUP_AP_SSID);
  if (already && WiFi.status() == WL_CONNECTED) return;  // 极端情况：已连上

  unsigned long lastUi = 0;
  while (WiFi.status() != WL_CONNECTED) {  // 配网成功即跳出存 NVS + 重启
    wm.process();  // 非阻塞配网服务：处理配网页请求 + captive portal
    ledTick();
    if (millis() - lastUi > 300) {
      lastUi = millis();
      // 波纹呼吸 1-2-3-3-2-1；有手机连上加速一档（"有人来了"的反馈）
      uint16_t step = WiFi.softAPgetStationNum() ? 150 : 300;
      uint8_t ph = (uint8_t)((millis() / step) % 6);
      uint8_t stage = ph < 3 ? (uint8_t)(ph + 1) : (uint8_t)(5 - ph);
      portalFrame(ui, 0, stage, "配网模式", "AP:" + String(SETUP_AP_SSID),
                  "IP 192.168.4.1",
                  "已连接 " + String(WiFi.softAPgetStationNum()), -1);
    }
    delay(10);
  }

  // 配网成功：同步存一份到本项目 NVS（loop 重连与 OLED 显示用），重启进正常模式
  saveWifiCreds(WiFi.SSID(), WiFi.psk());
  // 圆圈对勾 + 保存提示 1.5 秒后重启（和联网/配网屏同一套网格）
  portalFrame(ui, 1, 0, "保存成功", WiFi.SSID(), "重启中", "", -1);
  Serial.println("[WIFI] portal saved, reboot");
  delay(1500);
  ESP.restart();
}

// 读并消费“强制进配网”标志（requestWifiReset 置位，读一次即清零）
static bool consumeForcedPortal() {
  Preferences p;
  bool f = false;
  if (p.begin("filamentbox", false)) {
    f = p.getBool("force_portal", false);
    if (f) p.putBool("force_portal", false);
    p.end();
  }
  return f;
}

void requestWifiReset() {
  // 只删 WiFi 两个 key：同名空间下的 printer_*（打印机配置）原样保留
  Preferences p;
  if (p.begin("filamentbox", false)) {
    p.remove("ssid");
    p.remove("pass");
    p.putBool("force_portal", true);
    p.end();
  }
  WiFi.disconnect(true, true);  // 关 WiFi 并擦除 ESP-IDF 存的 STA 凭据，防止重启后自动连回旧网
  Serial.println("[WIFI] creds cleared, STA erased, reboot to portal");
  delay(300);
  ESP.restart();  // 不返回；重启后 consumeForcedPortal() 为真，直进 AP 配网
}

void ensureWifiOrPortal(OledUi* ui) {
  if (consumeForcedPortal()) {
    // 双保险：重启前可能残留旧 STA 凭据（如修复前的版本），这里再擦一次；
    // 正常连接失败分支不擦（要保留凭据等路由恢复）。
    WiFi.disconnect(true, true);
    portalFrame(ui, 0, 2, "已重置", "配网模式", "");
    delay(1500);
    runPortal(ui);  // 阻塞直到配网成功重启
    return;
  }
  String ssid, pass;
  loadWifiCreds(ssid, pass);
  if (connectWifiWith(ssid, pass, PORTAL_TIMEOUT_MS, ui)) return;
  // 15 秒连不上：圆圈叉静止 1.5 秒（和成功屏对称，不闪）后自动进 AP 配网
  for (uint8_t i = 0; i < 6; i++) {
    ledTick();
    portalFrame(ui, 2, 0, "连接失败", "", "配网模式", "", -1);
    delay(250);
  }
  runPortal(ui);  // 阻塞直到配网成功重启
}
