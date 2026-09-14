// wifi_portal.cpp — WiFi 配网（WiFiManager 非阻塞模式 + 自带 Captive Portal）
// 开机先用 NVS/默认凭据直连 15 秒；连不上则开配网 AP，手机会自动弹出配网页。
// 配网成功后凭据同步存一份到本项目 NVS（loop 重连与 OLED 显示用）。
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include "wifi_portal.h"
#include "config.h"
#include "oled_ui.h"

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

// OLED 通用屏：标题 + 内容 + 状态行（行数按死区自动裁剪到 5 行）
static void portalScreen(OledUi* ui, const String& title,
                         const String& l1 = "", const String& l2 = "",
                         const String& l3 = "", const String& l4 = "",
                         const String& st = "") {
  if (!ui) return;
  String lines[6] = {title, l1, l2, l3, l4, st};
  ui->showLines(lines, 6);
}

bool connectWifiWith(const String& ssid, const String& pass,
                     unsigned long timeoutMs, OledUi* ui) {
  WiFi.mode(WIFI_STA);
  Serial.print("[WIFI] begin ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long t0 = millis();
  uint8_t dots = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    String d = "";
    for (uint8_t i = 0; i <= dots % 3; i++) d += ".";
    portalScreen(ui, "FBX WiFi", ssid, "Connecting" + d, "", "", "");
    dots++;
    delay(500);
  }
  Serial.print("[WIFI] result ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
  return WiFi.status() == WL_CONNECTED;
}

static void runPortal(OledUi* ui) {
  Serial.println("[WIFI] start portal AP");
  WiFiManager wm;
  // 配网 AP 与之前手写页同名同 IP，老文档不用改
  wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                         IPAddress(255, 255, 255, 0));
  wm.setConfigPortalBlocking(false);  // 非阻塞：OLED 刷新由我们自己的循环做
  wm.setConfigPortalTimeout(0);       // 0 = 不超时，一直等到配好（与旧行为一致）
  wm.setCaptivePortalEnable(true);    // 手机连 AP 自动弹出配网页
  wm.setAPCallback([ui](WiFiManager*) {
    Serial.println("[WIFI] portal AP up");
    portalScreen(ui, "FBX WiFi Setup", "AP:FilamentBox-",
                 "IP 192.168.4.1", "Auto popup page", "", "");
  });

  bool already = wm.autoConnect(SETUP_AP_SSID);
  if (already && WiFi.status() == WL_CONNECTED) return;  // 极端情况：已连上

  unsigned long lastUi = 0;
  while (WiFi.status() != WL_CONNECTED) {  // 配网成功即跳出存 NVS + 重启
    wm.process();  // 非阻塞配网服务：处理配网页请求 + captive portal
    if (millis() - lastUi > 1000) {
      lastUi = millis();
      // 5 行（含死区裁剪）：标题 + AP 名 + IP + 操作指引 + 在线数
      portalScreen(ui, "FBX WiFi Setup",
                   "AP:FilamentBox-",
                   "IP 192.168.4.1",
                   "Auto popup page",
                   String("Clients:") + String(WiFi.softAPgetStationNum()));
    }
    delay(10);
  }

  // 配网成功：同步存一份到本项目 NVS（loop 重连与 OLED 显示用），重启进正常模式
  saveWifiCreds(WiFi.SSID(), WiFi.psk());
  portalScreen(ui, "FBX WiFi Setup", "Saved:", WiFi.SSID(), "reboot...", "", "");
  Serial.println("[WIFI] portal saved, reboot");
  delay(1500);
  ESP.restart();
}

void ensureWifiOrPortal(OledUi* ui) {
  String ssid, pass;
  loadWifiCreds(ssid, pass);
  if (connectWifiWith(ssid, pass, PORTAL_TIMEOUT_MS, ui)) return;
  // 15 秒连不上：自动进 AP 配网，OLED 同步显示
  portalScreen(ui, "FBX WiFi Setup", "WiFi conn fail", "AP setup mode", "", "", "");
  delay(1500);
  runPortal(ui);  // 阻塞直到配网成功重启
}
