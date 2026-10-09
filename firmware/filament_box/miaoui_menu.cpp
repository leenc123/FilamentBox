// miaoui_menu.cpp — 菜单 A 管理器实现（C++，桥接 MiaoUI 纯 C 内核）
#include <Arduino.h>
#include "miaoui_menu.h"
#include "config.h"
#include "wifi_portal.h"
#include "filament_map.h"

extern "C" {
#include "src/miaoui/ui_conf.h"
#include "src/miaoui/core/ui.h"
#include "src/miaoui/display/dispDriver.h"
#include "src/miaoui/widget/custom.h"
#include "src/miaoui/indev/indevDriver.h"
#include "src/miaoui/hal/fbx_button.h"
#include "src/miaoui/hal/fbx_actions.h"
}

// ui_conf.c 里的槽位快照 + 动态文本缓冲（MiaoUI 只存指针）
extern fbx_slot_t FbxSlots[4];
extern char FbxStatusText[24];
extern char FbxIpText[32];
// ui_conf.c 里的屏上写卡状态
extern int  FbxWSlot;
extern int  FbxWMat;
extern int  FbxWBrand;
extern int  FbxWColor;
extern char FbxWPresetName[13];
extern char FbxWColorHex[16];
extern char WMatName[16];
extern char WBrandName[16];
extern char WColorName[20];

static ui_t gMiaoUi;
static bool gMiaoActive = false;

extern "C" void Fbx_Reboot(ui_t *ui) {
  (void)ui;
  ESP.restart();
}

// 12 常用色英文短名（OLED 无中文字库，预览行用；与 SCREEN_COLORS 对应下标无关，按全表下标查）
static const char* screenColorEn(uint8_t fullIdx) {
  switch (fullIdx) {
    case 0: return "White";
    case 1: return "Black";
    case 2: return "Gray";
    case 5: return "Brown";
    case 8: return "Red";
    case 10: return "Orange";
    case 12: return "Yellow";
    case 13: return "Pink";
    case 15: return "Purple";
    case 17: return "Green";
    case 20: return "Cyan";
    case 23: return "Blue";
    default: return "Custom";
  }
}

extern "C" int fbxMatCount(void) {
  return (int)SCREEN_MAT_COUNT;
}

extern "C" int fbxColorCount(void) {
  return (int)SCREEN_COLOR_COUNT;
}

extern "C" const char* fbxMatName(int i) {
  if (i < 0 || (size_t)i >= SCREEN_MAT_COUNT) return "?";
  return SCREEN_MATS[i].family;
}

extern "C" const char* fbxBrandName(int b) {
  return (b == 0) ? "Bambu" : "Generic";
}

extern "C" const char* fbxColorName(int i) {
  if (i < 0 || (size_t)i >= SCREEN_COLOR_COUNT) return "?";
  uint8_t ci = SCREEN_COLORS[i];
  if (ci >= FILAMENT_COLOR_COUNT) return "?";
  return screenColorEn(ci);
}

extern "C" const char* fbxColorCnName(int i) {
  if (i < 0 || (size_t)i >= SCREEN_COLOR_COUNT) return "?";
  uint8_t ci = SCREEN_COLORS[i];
  if (ci >= FILAMENT_COLOR_COUNT) return "?";
  return FILAMENT_COLORS[ci].name;
}

extern "C" const char* fbxColorHex(int i) {
  if (i < 0 || (size_t)i >= SCREEN_COLOR_COUNT) return "?";
  uint8_t ci = SCREEN_COLORS[i];
  if (ci >= FILAMENT_COLOR_COUNT) return "?";
  return FILAMENT_COLORS[ci].rgb;
}

// 卡片读出的任意合法 HEX 查中文名：命中调色板返回中文名，否则 NULL（调用方回落显示 HEX）
extern "C" const char* fbxColorCnByRgb(const char *rgb6) {
  if (!rgb6) return (const char *)0;
  for (size_t i = 0; i < FILAMENT_COLOR_COUNT; i++) {
    const char *p = FILAMENT_COLORS[i].rgb;
    uint8_t k = 0;
    for (; k < 6; k++) {
      char a = rgb6[k], b = p[k];
      if (a >= 'a' && a <= 'f') a -= 32;
      if (b >= 'a' && b <= 'f') b -= 32;
      if (a != b) break;
    }
    if (k == 6) return FILAMENT_COLORS[i].name;
  }
  return (const char *)0;
}

// 屏上写卡步进函数：下标钳位 + 刷新预设短名/色值预览（纯 snprintf，无阻塞）
// Mat 选家族、Brand 选 0=Bambu官方/1=Generic通用，组合到代表预设；
// 预览行带 B/G 前缀以区分品牌；颜色走 12 常用色并显示英文短名+HEX
// （OLED 无中文字库，中文名只在网页端）。
extern "C" void Fbx_WritePreview(ui_t *ui) {
  (void)ui;
  int nm = fbxMatCount();
  int nc = fbxColorCount();
  if (nm < 1) nm = 1;
  if (nc < 1) nc = 1;
  if (FbxWSlot < 1) FbxWSlot = 1;
  if (FbxWSlot > NUM_SLOTS) FbxWSlot = NUM_SLOTS;
  if (FbxWMat < 0) FbxWMat = 0;
  if (FbxWMat >= nm) FbxWMat = nm - 1;
  if (FbxWBrand < 0) FbxWBrand = 0;
  if (FbxWBrand > 1) FbxWBrand = 1;
  if (FbxWColor < 0) FbxWColor = 0;
  if (FbxWColor >= nc) FbxWColor = nc - 1;
  const char* idx = (FbxWBrand == 0)
      ? SCREEN_MATS[FbxWMat].bambu
      : SCREEN_MATS[FbxWMat].generic;
  const FilamentInfo* info = lookupFilament(String(idx));
  String shown = info ? String(shortFilamentName(info)) : String(idx);
  if (shown.length() > 10) shown = shown.substring(0, 10);
  snprintf(FbxWPresetName, sizeof(FbxWPresetName), "%c %s",
           (FbxWBrand == 0) ? 'B' : 'G', shown.c_str());
  uint8_t ci = SCREEN_COLORS[FbxWColor];
  if (ci >= FILAMENT_COLOR_COUNT) ci = 0;
  snprintf(FbxWColorHex, sizeof(FbxWColorHex), "%s %s",
           screenColorEn(ci), FILAMENT_COLORS[ci].rgb);
  // Write 列表行名同步（MiaoUI 只存指针，改缓冲即改显示；中文经核心分流走点阵）
  snprintf(WMatName, sizeof(WMatName), "-材料 %s", fbxMatName(FbxWMat));
  snprintf(WBrandName, sizeof(WBrandName), "-品牌 %s", fbxBrandName(FbxWBrand));
  snprintf(WColorName, sizeof(WColorName), "-颜色 %s", fbxColorName(FbxWColor));
}

extern "C" void Fbx_WriteCard(ui_t *ui) {
  (void)ui;
  Fbx_WritePreview(NULL);
  fbxScreenWrite(FbxWSlot, FbxWMat, FbxWBrand, FbxWColor);
  // 结果弹窗：Write 页本身无状态位，不弹用户就看不到成功/失败
  // （如无读卡器时的 "WRITE n no reader"）；照 Fbx_ResetWifi 的全屏提示路子，
  // 显示约 1.2 秒后由框架退回 Write 页，远小于看门狗时限。
  // 状态前缀中文化（原因短语保持英文原文）；SENT->成功，FAIL->失败，WRITE->写卡。
  char raw[24], msg[24];
  snprintf(raw, sizeof(raw), "%s", fbxLastWriteMsg());
  if (strncmp(raw, "SENT ", 5) == 0) snprintf(msg, sizeof(msg), "成功 %s", raw + 5);
  else if (strncmp(raw, "FAIL ", 5) == 0) snprintf(msg, sizeof(msg), "失败 %s", raw + 5);
  else if (strncmp(raw, "WRITE ", 6) == 0) snprintf(msg, sizeof(msg), "写卡 %s", raw + 6);
  else snprintf(msg, sizeof(msg), "%s", raw);
  uint8_t c = 1;
  Disp_ClearBuffer();
  Disp_SetFont(UI_FONT);
  Disp_SetDrawColor(&c);
  Cn_DrawStr(0, 22, "结果:");
  Cn_DrawStr(0, 42, msg);
  Disp_SendBuffer();
  delay(1200);
}

extern "C" void Fbx_ResetWifi(ui_t *ui) {
  (void)ui;
  // 先给个全屏提示（约 1 秒，远小于看门狗时限），再清配置重启进 AP 配网
  uint8_t c = 1;
  Disp_ClearBuffer();
  Disp_SetFont(UI_FONT);
  Disp_SetDrawColor(&c);
  Cn_DrawStr(0, 22, "WiFi重置");
  Cn_DrawStr(0, 42, "重启配网");
  Disp_SendBuffer();
  delay(800);
  requestWifiReset();  // 内含 ESP.restart，不返回
}

bool miaouiSetupMenu() {
  fbxButtonInit();
  dispInit();  // 空操作（OledUi::begin 已初始化）；保留上游调用顺序
  for (int i = 0; i < 4; i++) {
    FbxSlots[i].tray = (uint8_t)(i + 1);
    FbxSlots[i].present = 0;
    FbxSlots[i].reader = 1;
    FbxSlots[i].name[0] = '\0';
    FbxSlots[i].color[0] = '\0';
  }
  snprintf(FbxStatusText, sizeof(FbxStatusText), "FBX Ready");
  snprintf(FbxIpText, sizeof(FbxIpText), "no ip");
  FbxWSlot = 1;
  FbxWMat = 0;
  FbxWBrand = 0;
  FbxWColor = 0;
  Fbx_WritePreview(NULL);  // 写卡预览缓冲初值（菜单树会存指针引用它们）
  MiaoUi_Setup(&gMiaoUi);
  // 开机动画：碰卡 + 字母滑入约 1.2 秒，任意键跳过（配网页随后接管屏幕）
  for (uint8_t f = 0; f <= 38; f++) {
    if (indevScan() != UI_ACTION_NONE) break;
    Draw_BootAnim(f);
    delay(30);
  }
  // 上游 INIT 状态要等第一次按键才画第一帧，之前 portal 的 connecting 残帧会一直留在屏上；
  // 直接推进 RUNING（RUNING + 无按键每 tick 重绘 + 跑动画），开机即见菜单。
  if (gMiaoUi.nowItem != NULL) gMiaoUi.menuState = UI_PAGE_RUNING;
  gMiaoActive = (gMiaoUi.nowItem != NULL);
  return gMiaoActive;
}

bool miaouiActive() { return gMiaoActive; }

void miaouiTick() {
  if (!gMiaoActive) return;
  ui_loop(&gMiaoUi);
}

void miaouiFeedAndTick(const String types[4], const uint8_t trays[4],
                       const String colors[4], const uint8_t readerPresent[4],
                       const String& status, const String& ip) {
  if (!gMiaoActive) return;
  for (uint8_t i = 0; i < 4; i++) {
    FbxSlots[i].tray = trays[i];
    FbxSlots[i].reader = readerPresent[i] ? 1 : 0;
    if (types[i].length() == 0) {
      FbxSlots[i].present = 0;
      FbxSlots[i].name[0] = '\0';
      FbxSlots[i].color[0] = '\0';
    } else {
      FbxSlots[i].present = 1;
      strncpy(FbxSlots[i].name, types[i].c_str(), sizeof(FbxSlots[i].name) - 1);
      FbxSlots[i].name[sizeof(FbxSlots[i].name) - 1] = '\0';
      String c6 = colors[i].substring(0, 6);
      strncpy(FbxSlots[i].color, c6.c_str(), sizeof(FbxSlots[i].color) - 1);
      FbxSlots[i].color[sizeof(FbxSlots[i].color) - 1] = '\0';
    }
  }
  if (status == "idle") {
    strncpy(FbxStatusText, "FBX Ready", sizeof(FbxStatusText) - 1);
  } else if (status == "WiFi retry") {
    strncpy(FbxStatusText, "网络重试", sizeof(FbxStatusText) - 1);
  } else {
    strncpy(FbxStatusText, status.c_str(), sizeof(FbxStatusText) - 1);
  }
  FbxStatusText[sizeof(FbxStatusText) - 1] = '\0';
  if (ip.length() == 0) {
    strncpy(FbxIpText, "no ip", sizeof(FbxIpText) - 1);
  } else {
    strncpy(FbxIpText, ip.c_str(), sizeof(FbxIpText) - 1);
  }
  FbxIpText[sizeof(FbxIpText) - 1] = '\0';
  ui_loop(&gMiaoUi);
}
