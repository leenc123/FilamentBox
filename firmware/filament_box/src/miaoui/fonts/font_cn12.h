#pragma once
// font_cn12.h - 12x12 Chinese subset (C linkage; row order matches u8g2_DrawXBMP: 2 bytes/row, LSB=left)
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CN12_W 12
#define CN12_H 12
// glyph lookup (row-major 24 bytes); NULL if missing (caller falls back, never draws garbage)
const uint8_t *cn12_get(uint32_t cp);
#ifdef __cplusplus
}
#endif