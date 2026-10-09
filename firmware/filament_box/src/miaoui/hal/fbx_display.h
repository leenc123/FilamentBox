#pragma once
// fbx_display.h — MiaoUI 显示 HAL（复用 oled_ui.cpp 的全局 U8g2 单例，不另起缓冲）
// dispInit 为空操作：Wire + 0x3C/0x3D 探测 + u8g2.begin() 已由 OledUi::begin() 完成。
// 纯 C 声明，供 miaoui/display/dispDriver.c 桥接调用；实现见 fbx_display.cpp。
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ui_t;
typedef struct ui_t ui_t;

void     HAL_dispInit(void);
void     HAL_Disp_ClearBuffer(void);
void     HAL_Disp_SendBuffer(void);
void     HAL_Disp_SetFont(const uint8_t *font);
void     HAL_Disp_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
uint16_t HAL_Disp_DrawStr(uint16_t x, uint16_t y, const char *str);
void     HAL_Disp_SetDrawColor(void *color);
void     HAL_Disp_DrawFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void     HAL_Disp_DrawRFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r);
void     HAL_Disp_DrawBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void     HAL_Disp_DrawRBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r);
void     HAL_Disp_DrawXBMP(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap);
void     HAL_Disp_SetContrast(ui_t *ui);
void     HAL_Disp_SetPowerSave(ui_t *ui);
uint8_t  HAL_Disp_GetBufferTileHeight(void);
uint8_t  HAL_Disp_GetBufferTileWidth(void);
uint8_t *HAL_Disp_GetBufferPtr(void);
void     HAL_Disp_SetClipWindow(uint16_t clip_x0, uint16_t clip_y0, uint16_t clip_x1, uint16_t clip_y1);
void     HAL_Disp_SetMaxClipWindow(void);
void     HAL_Disp_SetBufferCurrTileRow(uint8_t row);
uint16_t HAL_Disp_DrawUTF8(uint16_t x, uint16_t y, const char *str);
uint16_t HAL_Disp_GetUTF8Width(const char *str);
void     HAL_Disp_UpdateDisplayArea(uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th);

#ifdef __cplusplus
}
#endif
