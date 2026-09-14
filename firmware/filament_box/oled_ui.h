#pragma once
// oled_ui.h — SSD1306 状态显示（U8g2，128x64，6x10 大字体，顶部死区可配）
#include <Arduino.h>
#include <stdint.h>

class OledUi {
public:
  OledUi(uint8_t sda, uint8_t scl);
  // 初始化 I2C + 屏幕；无屏返回 false（调用方继续无屏运行）
  bool begin();
  bool ok() const { return _ok; }
  // 通用文本显示（行数按死区自动裁剪，每行超 21 字符截断），供配网等非槽位页面用
  void showLines(const String* lines, uint8_t n);
  // 槽位屏 5 行：4 槽 + 状态行（idle 显示 FBX Ready，有状态显示状态）；
  // 主页不显示本机 IP（IP 只在开机页和失败提示页出现）；
  // 内容不变不重刷由调用方判断
  void showSlots(const String types[4], const uint8_t trays[4],
                 const String colors[4], const String& status,
                 const String& ip = "");

private:
  uint8_t _sda, _scl;
  bool _ok = false;
};
