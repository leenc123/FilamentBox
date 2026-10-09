#pragma once
// fbx_actions.h — FilamentBox 菜单动作（C 链接，供 miaoui/ui_conf.c 的 AddItem 引用）。
#ifdef __cplusplus
extern "C" {
#endif

#include "../ui_conf.h"

// 重启设备（ONCE_FUNCTION 回调，实现在 miaoui_menu.cpp，调 ESP.restart()）
void Fbx_Reboot(ui_t *ui);
// 重置网络：清 WiFi 配置后重启进 AP 配网（实现在 miaoui_menu.cpp，不返回）
void Fbx_ResetWifi(ui_t *ui);

// 屏上写卡选择器上限（实现在 miaoui_menu.cpp，分别对应
// filament_map.h 的 SCREEN_MATS / SCREEN_COLORS；品牌固定 0=Bambu,1=Generic）
int  fbxMatCount(void);
int  fbxColorCount(void);
// 英文名（OLED 无中文字库，列表行名/选项浏览器用；实现在 miaoui_menu.cpp）
const char* fbxMatName(int i);    // 家族名，如 "PA-CF"
const char* fbxBrandName(int b);  // "Bambu" / "Generic"
const char* fbxColorName(int i);  // 12 常用色英文短名，如 "Red"
// 中文名（OLED 点阵字库覆盖；实现在 miaoui_menu.cpp）
const char* fbxColorCnName(int i);          // 12 常用色中文名，如 "红色"
const char* fbxColorHex(int i);             // 12 常用色 RRGGBB，如 "FF0000"
const char* fbxColorCnByRgb(const char *rgb6);  // HEX 查中文名，无对应返回 NULL
void Fbx_WritePreview(ui_t *ui);  // 步进函数：clamp 下标 + 刷新两个预览缓冲
void Fbx_WriteCard(ui_t *ui);     // 确认写入：调 .ino 的 fbxScreenWrite，全程约 1-3 秒

#ifdef __cplusplus
}
#endif
