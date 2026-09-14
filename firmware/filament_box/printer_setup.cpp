// printer_setup.cpp
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "printer_setup.h"
#include "config.h"
#include "oled_ui.h"

static WebServer setupServer(80);
static PrinterCfg* g_cfg = nullptr;
static String setupMsg;

void loadPrinterCfg(PrinterCfg& cfg) {
  Preferences p;
  if (p.begin("filamentbox", true)) {
    cfg.ip = p.getString("printer_ip", PRINTER_IP);
    cfg.serial = p.getString("printer_sn", PRINTER_SERIAL);
    cfg.code = p.getString("printer_code", PRINTER_ACCESS_CODE);
    cfg.dbg = p.getBool("printer_dbg", false);
    p.end();
  } else {
    cfg.ip = PRINTER_IP;
    cfg.serial = PRINTER_SERIAL;
    cfg.code = PRINTER_ACCESS_CODE;
    cfg.dbg = false;
  }
  cfg.ip.trim();
  cfg.serial.trim();
  cfg.code.trim();
}

void savePrinterCfg(const PrinterCfg& cfg) {
  Preferences p;
  if (p.begin("filamentbox", false)) {
    p.putString("printer_ip", cfg.ip);
    p.putString("printer_sn", cfg.serial);
    p.putString("printer_code", cfg.code);
    p.putBool("printer_dbg", cfg.dbg);
    p.end();
  }
}

bool checkPrinterReachable(const String& ip, uint16_t port, unsigned long timeoutMs) {
  WiFiClient probe;
  probe.setTimeout(timeoutMs / 1000 + 1);
  bool ok = probe.connect(ip.c_str(), port, timeoutMs);
  probe.stop();
  return ok;
}

// 内联轻量样式（无外部资源，局域网/AP 下也能用）：卡片居中 + 大输入框 + 大按钮
static const char* PAGE_STYLE =
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
  ".check{display:flex;gap:8px;align-items:flex-start;margin:14px 0 0;font-size:14px}"
  ".btn{margin-top:16px;width:100%;padding:11px;border:0;border-radius:8px;"
  "background:#1f6feb;color:#fff;font-size:16px}"
  ".msg{padding:10px 12px;border-radius:8px;font-size:14px;margin:12px 0 0;"
  "background:#fef3c7;color:#92400e}"
  ".msg.ok{background:#dcfce7;color:#166534}"
  ".hint{color:#6b7280;font-size:13px;margin:8px 0 0}"
  "a{color:#1f6feb}"
  "</style>";

// 回填到 value="" 前做转义，防止引号/尖括号破坏页面结构
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

static String setupForm() {
  String h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>FilamentBox 打印机配置</title>";
  h += PAGE_STYLE;
  h += "</head><body><div class='card'>"
       "<h3>打印机配置</h3>"
       "<p class='sub'>FilamentBox · 当前端口：" + String(g_cfg->mqttPort()) +
       (g_cfg->dbg ? "（明文）" : "（TLS）") + "</p>";
  if (setupMsg.length()) h += "<p class='msg'>" + esc(setupMsg) + "</p>";
  h += "<form method='POST' action='/save'>"
       "<label>打印机 IP</label>"
       "<input class='field' name='ip' value='" + esc(g_cfg->ip) +
       "' maxlength='15' inputmode='decimal' placeholder='如 192.168.1.100'>"
       "<label>序列号</label>"
       "<input class='field' name='sn' value='" + esc(g_cfg->serial) +
       "' maxlength='32'>"
       "<label>访问码</label>"
       "<input class='field' name='code' value='" + esc(g_cfg->code) +
       "' maxlength='32'>"
       "<label class='check'><input type='checkbox' name='dbg' value='1'" +
       String(g_cfg->dbg ? " checked" : "") +
       "> 调试模式（明文 1883，仅排查用）</label>"
       "<input class='btn' type='submit' value='检测并保存'></form>"
       "<p class='hint'>保存前会先直连所选端口，通了才存；不通不覆盖旧配置。</p>"
       "<p class='hint'>访问码在打印机屏幕 -&gt; 设置 -&gt; 局域网访问码查看。</p>"
       "<p class='hint'>注意：1883 明文只在部分老固件上开着，新固件可能已关闭；"
       "仅排查时打开，平时保持关闭。</p>"
       "</div></body></html>";
  return h;
}

static void handleSetupRoot() {
  setupMsg = "";
  setupServer.send(200, "text/html; charset=utf-8", setupForm());
}

static void handleSetupSave() {
  if (!setupServer.hasArg("ip") || !setupServer.hasArg("sn") ||
      !setupServer.hasArg("code")) {
    setupMsg = "三项都要填";
    setupServer.send(400, "text/html; charset=utf-8", setupForm());
    return;
  }
  String ip = setupServer.arg("ip");
  String sn = setupServer.arg("sn");
  String code = setupServer.arg("code");
  bool dbg = setupServer.hasArg("dbg");  // 复选框：勾上才有该参数
  ip.trim();
  sn.trim();
  code.trim();
  if (ip.length() == 0 || sn.length() == 0 || code.length() == 0) {
    setupMsg = "三项都要填";
    setupServer.send(400, "text/html; charset=utf-8", setupForm());
    return;
  }
  uint16_t port = dbg ? 1883 : 8883;
  // 可达性检测：选中端口不通不存，旧配置继续用
  if (!checkPrinterReachable(ip, port)) {
    setupMsg = "IP/端口不可达（" + ip + ":" + String(port) +
               "），检查打印机 IP 与局域网模式，旧配置未覆盖";
    setupServer.send(200, "text/html; charset=utf-8", setupForm());
    return;
  }
  g_cfg->ip = ip;
  g_cfg->serial = sn;
  g_cfg->code = code;
  g_cfg->dbg = dbg;
  savePrinterCfg(*g_cfg);
  setupMsg = "";
  String ok = "<!doctype html><html><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>FilamentBox 打印机配置</title>";
  ok += PAGE_STYLE;
  ok += "</head><body><div class='card'>"
        "<h3>打印机配置</h3>"
        "<p class='msg ok'>已保存，" + String(port) + " 可达，下次换卡自动用新配置推送。</p>"
        "<p class='hint'><a href='/'>进入管理首页</a> · "
        "<a href='/setup'>返回修改</a></p>"
        "</div></body></html>";
  setupServer.send(200, "text/html; charset=utf-8", ok);
}

void startPrinterSetup(PrinterCfg& cfg) {
  g_cfg = &cfg;
  setupServer.on("/setup", handleSetupRoot);
  setupServer.on("/save", HTTP_POST, handleSetupSave);
  setupServer.begin();
}

void printerSetupHandle() {
  setupServer.handleClient();
}

WebServer& setupWebServer() {
  return setupServer;
}

void showPrinterHint(OledUi* ui) {
  if (!ui) return;
  String lines[6] = {
    "Printer cfg?",
    "Open:",
    WiFi.localIP().toString(),
    "/setup save IP",
    "",
    ""
  };
  ui->showLines(lines, 6);
}
