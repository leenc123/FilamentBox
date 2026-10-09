// oled_ui.cpp
#include <Wire.h>
#include <U8g2lib.h>
#include "oled_ui.h"
#include "config.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oledShareU8g2() { return u8g2; }

OledUi::OledUi(uint8_t sda, uint8_t scl) : _sda(sda), _scl(scl) {}

bool OledUi::begin() {
  Wire.begin(_sda, _scl);
  // 依次探测 0x3C / 0x3D（市面两种模块都兼容），无应答视为无屏，不阻塞
  uint8_t found = 0;
  const uint8_t candidates[2] = {OLED_ADDR, 0x3D};
  for (uint8_t i = 0; i < 2; i++) {
    Wire.beginTransmission(candidates[i]);
    if (Wire.endTransmission() == 0) { found = candidates[i]; break; }
  }
  if (!found) {
    Serial.println("[OLED] no screen found at 0x3C/0x3D, run headless");
    _ok = false;
    return false;
  }
  u8g2.setI2CAddress(found << 1);  // U8g2 用 8 位地址
  u8g2.begin();
  // 6x10 大字体（10px 高）配 10px 行距，行间不重叠；行数由 showLines 按死区裁剪
  u8g2.setFont(u8g2_font_6x10_tf);
  Serial.print("[OLED] found at 0x");
  Serial.println(found, HEX);
  _ok = true;
  return true;
}

void OledUi::showLines(const String* lines, uint8_t n) {
  if (!_ok) return;
  // 死区偏移：首行基线 = TOP_DEAD + 10（6x10 字顶再留 1px），10px 行距；
  // TOP=10 时 5 行收在 y=20~60，顶部坏区一个像素都不用
  const uint8_t y0 = OLED_TOP_DEAD + 10;
  uint8_t maxRows = (y0 > 63) ? 0 : (63 - y0) / 10 + 1;
  if (n > maxRows) n = maxRows;
  u8g2.clearBuffer();
  char buf[24];
  for (uint8_t i = 0; i < n; i++) {
    snprintf(buf, sizeof(buf), "%s", lines[i].c_str());
    buf[21] = '\0';  // 6px 字体 128 宽约 21 字符，铺满整行
    u8g2.drawStr(0, y0 + i * 10, buf);
  }
  u8g2.sendBuffer();
}

void OledUi::showSlots(const String types[4], const uint8_t trays[4],
                       const String colors[4], const String& status,
                       const String& ip) {
  (void)ip;  // 主页不再常驻 IP：IP 只在开机页和失败提示页出现
  // 槽位屏固定 5 行：4 槽 + 状态行。idle 时状态行显示 Ready，
  // 有 SENT/FAIL/retry 等状态时显示状态（推送反馈优先）。
  String lines[5];
  char line[24];
  for (uint8_t i = 0; i < 4; i++) {
    if (types[i].length() == 0) {
      snprintf(line, sizeof(line), "%d: -- empty", trays[i]);
    } else {
      // 短名左对齐 12 宽（如 PLA Matte）+ RRGGBB，全行 21 字符铺满
      snprintf(line, sizeof(line), "%d:%-12s %s", trays[i],
               types[i].c_str(), colors[i].substring(0, 6).c_str());
    }
    lines[i] = String(line);
  }
  if (status == "idle") {
    lines[4] = "FBX Ready";
  } else {
    lines[4] = status;
  }
  showLines(lines, 5);
}
