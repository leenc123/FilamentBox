#pragma once
// web_dash.h — 正常联网模式下的网页后台：管理首页（四槽色块）+ 网页写卡
// 纯 HTML 拼装；路由注册与读写推送动作由 filament_box.ino 完成
#include <Arduino.h>

struct DashSlot {
  String type;   // 空字符串 = 无卡
  String color;  // RRGGBB（6 位，给色块用）
};

// 管理首页（slots 快照 + 状态行）；整页 5 秒定时刷新
String dashHome(const DashSlot slots[4], const String& status);
// 写卡表单（msg 为空不显示横幅；ok 决定横幅颜色）
String dashWriteForm(const String& msg, bool ok);
// 写卡结果页
String dashWriteResult(bool ok, const String& msg);
// 日志页（首屏 80 行 + 1 秒 fetch 轮询）；纯文本全量（复制排查用）
String dashLogPage(const String& status, uint32_t cursor);
String dashLogText();
