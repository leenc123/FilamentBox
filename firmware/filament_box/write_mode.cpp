// write_mode.cpp
#include <WiFi.h>
#include <WebServer.h>
#include "write_mode.h"
#include "config.h"
#include "filament_map.h"
#include "rfid_store.h"

static WebServer server(80);
static RfidReader* g_reader = nullptr;

bool shouldEnterWriteMode(uint8_t buttonPin, unsigned long holdMs) {
  pinMode(buttonPin, INPUT_PULLUP);
  if (digitalRead(buttonPin) == HIGH) return false;
  unsigned long t0 = millis();
  while (digitalRead(buttonPin) == LOW) {
    if (millis() - t0 >= holdMs) return true;
    delay(50);
  }
  return false;
}

// 与 /setup 页同风格的内联轻量样式（无外部资源，AP 下也能用）
static const char* WRITE_STYLE =
  "<style>"
  "*{box-sizing:border-box}"
  "body{margin:0;padding:16px;background:#eef1f6;color:#1f2937;"
  "font-family:-apple-system,'Segoe UI',Roboto,'PingFang SC','Microsoft YaHei',sans-serif}"
  ".card{max-width:480px;margin:24px auto;background:#fff;border-radius:12px;"
  "padding:20px;box-shadow:0 2px 12px rgba(0,0,0,.08)}"
  "h3{margin:0;font-size:18px}"
  ".sub{margin:4px 0 0;color:#6b7280;font-size:13px}"
  "label{display:block;margin:12px 0 4px;font-size:14px;color:#374151}"
  ".field{width:100%;padding:9px 10px;font-size:15px;border:1px solid #d1d5db;"
  "border-radius:8px;background:#f9fafb}"
  ".field:focus{outline:none;border-color:#2563eb;background:#fff}"
  ".cgrid{display:grid;grid-template-columns:repeat(7,1fr);gap:8px;margin-top:4px}"
  ".csw{display:block;text-align:center;margin:0}"
  ".csw input{display:none}"
  ".csw span{display:block;height:38px;border-radius:8px;border:1px solid #d1d5db}"
  ".csw i{display:block;font-style:normal;font-size:11px;color:#4b5563;margin-top:2px}"
  ".csw input:checked+span{outline:2px solid #1f6feb;outline-offset:1px}"
  ".btn{margin-top:16px;width:100%;padding:11px;border:0;border-radius:8px;"
  "background:#1f6feb;color:#fff;font-size:16px}"
  ".msg{padding:10px 12px;border-radius:8px;font-size:14px;margin:12px 0 0;"
  "background:#fef3c7;color:#92400e}"
  ".msg.ok{background:#dcfce7;color:#166534}"
  "</style>";

static String esc(const String& s) {
  String o;
  o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '\'') o += "&#39;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

static String buildForm(const String& msg = "") {
  String h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>FilamentBox 写卡</title>";
  h += WRITE_STYLE;
  h += "</head><body><div class='card'>"
       "<h3>写卡</h3>"
       "<p class='sub'>FilamentBox · 把空白卡放到所选槽位的读卡器上，再点 Write</p>";
  if (msg.length()) {
    bool ok = msg.startsWith("OK");
    h += "<p class='msg" + String(ok ? " ok" : "") + "'>" + esc(msg) + "</p>";
  }
  h += "<form method='POST' action='/write'>"
       "<label>槽位</label><select class='field' name='slot'>";
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    h += "<option value='" + String(i) + "'>槽位 " + String(trayDisplayNo(i)) +
         "</option>";
  }
  h += "</select><label>类型（拓竹官方预设）</label>"
       "<select class='field' name='type'>";
  h += filamentTypeOptions();
  h += "</select><label>颜色（点选色块）</label>";
  h += filamentColorSwatches();
  h += "<input class='btn' type='submit' value='Write'></form>"
       "</div></body></html>";
  return h;
}

static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", buildForm());
}

static void handleWrite() {
  if (!server.hasArg("slot") || !server.hasArg("type") || !server.hasArg("color")) {
    server.send(400, "text/html; charset=utf-8", buildForm("bad args"));
    return;
  }
  int slot = server.arg("slot").toInt();
  String type = server.arg("type");
  String color = server.arg("color");
  color.toUpperCase();
  type.trim();
  color.trim();

  if (slot < 0 || slot >= NUM_SLOTS) {
    server.send(400, "text/html; charset=utf-8", buildForm("bad slot"));
    return;
  }
  const FilamentInfo* winfo = lookupFilament(type);
  if (!winfo) {
    server.send(400, "text/html; charset=utf-8", buildForm("unknown type"));
    return;
  }
  if (!isPaletteColor(color)) {
    server.send(400, "text/html; charset=utf-8", buildForm("bad color: use palette"));
    return;
  }
  uint8_t b4[16], b5[16];
  if (!encodeCard(type, color, b4, b5)) {
    server.send(400, "text/html; charset=utf-8", buildForm("encode fail"));
    return;
  }
  if (!g_reader->writeSlot((uint8_t)slot, b4, b5)) {
    server.send(500, "text/html; charset=utf-8",
                buildForm("write failed: 卡是否放在该槽读卡器上?"));
    return;
  }
  server.send(200, "text/html; charset=utf-8",
              buildForm("OK 槽位 " + String(trayDisplayNo((uint8_t)slot)) + " " + String(winfo->name) +
                        " " + color + "（复位回到正常模式刷卡验证）"));
}

void runWriteMode(RfidReader& reader) {
  g_reader = &reader;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(SETUP_AP_SSID);
  server.on("/", handleRoot);
  server.on("/write", HTTP_POST, handleWrite);
  server.begin();
  while (true) {
    server.handleClient();
    delay(2);
  }
}
