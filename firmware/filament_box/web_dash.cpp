// web_dash.cpp
#include "web_dash.h"
#include "config.h"
#include "filament_map.h"

// 与 /setup、写卡 AP 页同风格的内联轻量样式 + 槽位色块
static const char* DASH_STYLE =
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
  ".slots{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}"
  ".slot{border:1px solid #e5e7eb;border-radius:10px;overflow:hidden}"
  ".sw{height:34px}"
  ".cap{padding:6px 8px;font-size:13px}"
  ".cap span{color:#6b7280;font-size:12px}"
  ".row2{display:flex;gap:8px;margin-top:14px}"
  ".btn2{flex:1;text-align:center;padding:10px;border-radius:8px;"
  "background:#1f6feb;color:#fff;text-decoration:none;font-size:15px}"
  ".ghost{background:#e5e7eb;color:#111827}"
  ".msg{padding:10px 12px;border-radius:8px;font-size:14px;margin:12px 0 0;"
  "background:#fef3c7;color:#92400e}"
  ".msg.ok{background:#dcfce7;color:#166534}"
  ".hint{color:#6b7280;font-size:13px;margin:8px 0 0}"
  "a{color:#1f6feb}"
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

String dashHome(const DashSlot slots[4], const String& status) {
  String h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<meta http-equiv='refresh' content='5'>"
             "<title>FilamentBox</title>";
  h += DASH_STYLE;
  h += "</head><body><div class='card'>"
       "<h3>FilamentBox</h3>"
       "<p class='sub'>状态：" + esc(status) + "</p>"
       "<div class='slots'>";
  for (uint8_t i = 0; i < 4; i++) {
    if (slots[i].type.length() == 0) {
      h += "<div class='slot'><div class='sw' style='background:#e5e7eb'></div>"
           "<div class='cap'>" + String(trayDisplayNo(i)) + ": -- empty</div></div>";
    } else {
      h += "<div class='slot'><div class='sw' style='background:#" +
           esc(slots[i].color) + "'></div>"
           "<div class='cap'>" + String(trayDisplayNo(i)) + ": " + esc(slots[i].type) +
           "<br><span>" + esc(slots[i].color) + "</span></div></div>";
    }
  }
  h += "</div>"
       "<div class='row2'><a class='btn2' href='/write'>写卡</a>"
       "<a class='btn2 ghost' href='/setup'>打印机配置</a></div>"
       "</div></body></html>";
  return h;
}

String dashWriteForm(const String& msg, bool ok) {
  String h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>FilamentBox 写卡</title>";
  h += DASH_STYLE;
  h += "</head><body><div class='card'>"
       "<h3>写卡</h3>"
       "<p class='sub'>把空白卡放到所选槽位的读卡器上，再点 Write"
       "（写完自动推送到对应 AMS 槽验证）</p>";
  if (msg.length()) {
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
       "<p class='hint'><a href='/'>← 返回管理首页</a></p>"
       "</div></body></html>";
  return h;
}

String dashWriteResult(bool ok, const String& msg) {
  String h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>FilamentBox 写卡</title>";
  h += DASH_STYLE;
  h += "</head><body><div class='card'>"
       "<h3>写卡</h3>"
       "<p class='msg" + String(ok ? " ok" : "") + "'>" + esc(msg) + "</p>"
       "<div class='row2'><a class='btn2' href='/write'>继续写卡</a>"
       "<a class='btn2 ghost' href='/'>管理首页</a></div>"
       "</div></body></html>";
  return h;
}
