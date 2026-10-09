// fbx_display.cpp — MiaoUI 显示 HAL 实现：直调共享 U8g2 单例（oledShareU8g2）。
// 注意：u8g2 的 C++ setFont(const uint8_t*) 与 MiaoUI 自带字体表（fonts.c）兼容。
#include "fbx_display.h"
#include "../../../oled_ui.h"
#include "../ui_conf.h"

void HAL_dispInit(void) {
  // 空操作：OledUi::begin() 已完成 Wire.begin + 地址探测 + u8g2.begin()。
}

void HAL_Disp_ClearBuffer(void) { oledShareU8g2().clearBuffer(); }
void HAL_Disp_SendBuffer(void) { oledShareU8g2().sendBuffer(); }
void HAL_Disp_SetFont(const uint8_t *font) { oledShareU8g2().setFont(font); }
void HAL_Disp_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  oledShareU8g2().drawLine(x1, y1, x2, y2);
}
uint16_t HAL_Disp_DrawStr(uint16_t x, uint16_t y, const char *str) {
  return oledShareU8g2().drawStr(x, y, str);
}
void HAL_Disp_SetDrawColor(void *color) { oledShareU8g2().setDrawColor(*(uint8_t *)color); }
void HAL_Disp_DrawFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  oledShareU8g2().drawFrame(x, y, w, h);
}
void HAL_Disp_DrawRFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r) {
  oledShareU8g2().drawRFrame(x, y, w, h, r);
}
void HAL_Disp_DrawBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  oledShareU8g2().drawBox(x, y, w, h);
}
void HAL_Disp_DrawRBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r) {
  oledShareU8g2().drawRBox(x, y, w, h, r);
}
void HAL_Disp_DrawXBMP(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap) {
  oledShareU8g2().drawXBMP(x, y, w, h, bitmap);
}
void HAL_Disp_SetContrast(ui_t *ui) {
  if (!ui || !ui->nowItem || !ui->nowItem->element || !ui->nowItem->element->data ||
      !ui->nowItem->element->data->ptr) {
    return;
  }
  oledShareU8g2().setContrast(*(uint8_t *)ui->nowItem->element->data->ptr);
}
void HAL_Disp_SetPowerSave(ui_t *ui) {
  if (!ui || !ui->nowItem || !ui->nowItem->element || !ui->nowItem->element->data ||
      !ui->nowItem->element->data->ptr) {
    return;
  }
  oledShareU8g2().setPowerSave(*(uint8_t *)ui->nowItem->element->data->ptr);
}
uint8_t HAL_Disp_GetBufferTileHeight(void) { return oledShareU8g2().getBufferTileHeight(); }
uint8_t HAL_Disp_GetBufferTileWidth(void) { return oledShareU8g2().getBufferTileWidth(); }
uint8_t *HAL_Disp_GetBufferPtr(void) { return oledShareU8g2().getBufferPtr(); }
void HAL_Disp_SetClipWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  oledShareU8g2().setClipWindow(x0, y0, x1, y1);
}
void HAL_Disp_SetMaxClipWindow(void) { oledShareU8g2().setMaxClipWindow(); }
void HAL_Disp_SetBufferCurrTileRow(uint8_t row) { oledShareU8g2().setBufferCurrTileRow(row); }
uint16_t HAL_Disp_DrawUTF8(uint16_t x, uint16_t y, const char *str) {
  return oledShareU8g2().drawUTF8(x, y, str);
}
uint16_t HAL_Disp_GetUTF8Width(const char *str) { return oledShareU8g2().getUTF8Width(str); }
void HAL_Disp_UpdateDisplayArea(uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th) {
  oledShareU8g2().updateDisplayArea(tx, ty, tw, th);
}
