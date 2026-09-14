#pragma once
// rfid_store.h — 卡编解码（与 Python 版 rfid_store.py 同格式，可互通）
// Mifare Classic 1K 扇区1：Block4 = 类型 ASCII + 0x00 补齐 16B；
// Block5 = [R,G,B,0xFF,0x00,0x00,CRC8,0x00*9]
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

inline uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) crc = ((crc << 1) ^ 0x07) & 0xFF;
      else crc = (crc << 1) & 0xFF;
    }
  }
  return crc;
}

// 编码：成功返回 true 并填满 b4/b5（各 16B）；类型超 12B 或颜色非法返回 false
inline bool encodeCard(const String& trayType, const String& colorRgba,
                       uint8_t b4[16], uint8_t b5[16]) {
  if (trayType.length() == 0 || trayType.length() > 12) return false;
  String c = colorRgba;
  c.toUpperCase();
  if (c.length() != 8) return false;
  uint8_t rgb[3];
  for (uint8_t i = 0; i < 3; i++) {
    char hi = c[i * 2], lo = c[i * 2 + 1];
    auto nib = [](char ch) -> int8_t {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
      return -1;
    };
    int8_t h = nib(hi), l = nib(lo);
    if (h < 0 || l < 0) return false;
    rgb[i] = (uint8_t)((h << 4) | l);
  }
  memset(b4, 0x00, 16);
  memcpy(b4, trayType.c_str(), trayType.length());
  uint8_t body[6] = {rgb[0], rgb[1], rgb[2], 0xFF, 0x00, 0x00};
  memset(b5, 0x00, 16);
  memcpy(b5, body, 6);
  b5[6] = crc8(body, 6);
  return true;
}

// 解码：CRC 错返回 false；成功输出 type（String）与 colorRgba（大写 8 位）
inline bool decodeCard(const uint8_t b4[16], const uint8_t b5[16],
                       String& trayType, String& colorRgba) {
  if (crc8(b5, 6) != b5[6]) return false;
  char t[13];
  memcpy(t, b4, 12);
  t[12] = '\0';
  trayType = String(t);
  char c[9];
  snprintf(c, sizeof(c), "%02X%02X%02XFF", b5[0], b5[1], b5[2]);
  colorRgba = String(c);
  return true;
}
