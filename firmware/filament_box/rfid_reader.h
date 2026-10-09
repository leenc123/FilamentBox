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
  // 该槽位读卡器是否存在（上电探测 + 双向热插拔：缺席每 2 秒重试；
  // 在位时连续 4 轮 select 失败则用 VersionReg 探针仲裁，探针不通过才降级）
  bool present(uint8_t i) const { return i < _n && _present[i]; }
  // 读指定槽位：成功返回 true 并填满 b4/b5（各 16B）；无卡/认证失败/模块缺席返回 false
  // Classic 1K 走扇区1 Block4/5；NTAG213/215/216 走用户页 4-11（8 页共 32B，内容格式与 b4/b5 完全一致）
  bool readSlot(uint8_t i, uint8_t b4[16], uint8_t b5[16]);
  // 写指定槽位：Classic 用 KeyB 全 FF；NTAG 无需认证直接写页；写完halt前重读校验一致返回 true
  bool writeSlot(uint8_t i, const uint8_t b4[16], const uint8_t b5[16]);

private:
  // 读 VersionReg 两次，一致且非 0x00/0xFF 视为模块存在（MISO 悬空时读数随机/全 1）
  bool probeSlot(uint8_t i);
  // 缺席槽位节流重试：到点则 probe 一次，成功转 present
  bool ensurePresent(uint8_t i);
  bool selectCard(uint8_t i, MFRC522::Uid& uid);
  bool isUltralight(MFRC522* r);
  uint8_t _sck, _mosi, _miso, _rst, _n;
  const uint8_t* _cs;
  MFRC522* _pcs[8];  // 上限 8 路，当前用 4 路
  bool _present[8];
  unsigned long _retryMs[8];
  // 连续 select 失败计数（正常无卡也会失败，满阈值后用 VersionReg 探针仲裁，
  // 探针不通过才降级 _present，避免误杀）
  uint8_t _failStreak[8];
};
