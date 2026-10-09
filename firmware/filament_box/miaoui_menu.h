#pragma once
// miaoui_menu.h — FilamentBox 菜单 A 管理器（MiaoUI 接管正常态 OLED）
// 失败回落：miaouiSetupMenu() 返回 false 时调用方继续用老 OledUi 直刷屏。
#include <Arduino.h>
#include <stdint.h>

// 初始化三键 + MiaoUI 菜单树；成功返回 true（此后 OLED 归 MiaoUI 所有）。
bool miaouiSetupMenu();
// MiaoUI 是否已接管显示。
bool miaouiActive();
// 喂槽位/读卡器在位/状态/IP 快照并跑一步 ui_loop（数据变化时调一次即可）。
void miaouiFeedAndTick(const String types[4], const uint8_t trays[4],
                       const String colors[4], const uint8_t readerPresent[4],
                       const String& status, const String& ip);
// 只跑一步 ui_loop（按键扫描+动画），等待间隙每 ~30ms 调一次；无堆分配。
void miaouiTick();
// 屏上 Write 菜单执行写卡（.ino 实现：材料×品牌解代表预设 + 12常用色解色值，
// 复用编码+写入+推送验证链，结果进 statusText）
bool fbxScreenWrite(int slot1, int matIdx, int brandIdx, int colorIdx);
// 取上次屏上写卡结果（.ino 的 statusText 快照，调用方须立即拷贝；供结果弹窗用）
const char* fbxLastWriteMsg();
