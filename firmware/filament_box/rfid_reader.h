#pragma once
// rfid_reader.h — N 路 RC522 驱动（ESP32 VSPI 共用总线 + 独立 CS，逐路选通）
#include <Arduino.h>
#include <stdint.h>
#include <MFRC522.h>

class RfidReader {
public:
  RfidReader(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t rst,
             const uint8_t* csPins, uint8_t nslots);
  void begin();
  uint8_t count() const { return _n; }
  // 读指定槽位：成功返回 true 并填满 b4/b5（各 16B）；无卡/认证失败返回 false
  bool readSlot(uint8_t i, uint8_t b4[16], uint8_t b5[16]);
  // 写指定槽位（KeyB 全 FF）：写完重读校验一致返回 true
  bool writeSlot(uint8_t i, const uint8_t b4[16], const uint8_t b5[16]);

private:
  bool selectCard(uint8_t i, MFRC522::Uid& uid);
  uint8_t _sck, _mosi, _miso, _rst, _n;
  const uint8_t* _cs;
  MFRC522* _pcs[8];  // 上限 8 路，当前用 4 路
};
