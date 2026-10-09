// rfid_reader.cpp
#include <SPI.h>
#include "rfid_reader.h"
#include "log_ring.h"

RfidReader::RfidReader(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t rst,
                       const uint8_t* csPins, uint8_t nslots)
  : _sck(sck), _mosi(mosi), _miso(miso), _rst(rst), _n(nslots), _cs(csPins) {
  if (_n > 8) _n = 8;
  for (uint8_t i = 0; i < 8; i++) _pcs[i] = nullptr;
}

void RfidReader::begin() {
  SPI.begin(_sck, _miso, _mosi);  // SS 脚由各 MFRC522 实例自行管理
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, HIGH);
  for (uint8_t i = 0; i < _n; i++) {
    _present[i] = false;
    _retryMs[i] = 0;
    _failStreak[i] = 0;
    _pcs[i] = new MFRC522(_cs[i], _rst);
    if (!_pcs[i]) continue;
    _pcs[i]->PCD_Init();
    // 缺模块时 PCD_Init 也不阻塞（纯 SPI 读写，MISO 悬空读回垃圾值），probe 判定即可
    _present[i] = probeSlot(i);
    _retryMs[i] = millis();
    logLine("[RFID] slot " + String(i) + (_present[i] ? ": reader present" : ": no reader (retry in background)"));
  }
}

bool RfidReader::probeSlot(uint8_t i) {
  if (i >= _n || !_pcs[i]) return false;
  uint8_t v1 = _pcs[i]->PCD_ReadRegister(MFRC522::VersionReg);
  uint8_t v2 = _pcs[i]->PCD_ReadRegister(MFRC522::VersionReg);
  // 0x00/0xFF = 总线无应答；两次一致才认（MISO 悬空时读数随机跳变）
  return v1 == v2 && v1 != 0x00 && v1 != 0xFF;
}

bool RfidReader::ensurePresent(uint8_t i) {
  if (i >= _n || !_pcs[i]) return false;
  if (_present[i]) return true;
  // 缺席槽位每 2 秒重探一次，面包板上热插上模块后自动识别，无需重启
  if (millis() - _retryMs[i] < 2000) return false;
  _retryMs[i] = millis();
  _pcs[i]->PCD_Init();
  _present[i] = probeSlot(i);
  if (_present[i]) logLine("[RFID] slot " + String(i) + ": reader attached");
  return _present[i];
}

// 寻卡 + 防冲突 + 选中，成功输出 uid
// 注意：读/写完都会 Halt，Halt 后的卡不再响应 REQA，必须用 WUPA 唤醒，
// 否则“卡一直放着”时第二次 select 必失败（写卡点 Write 前主循环已 Halt 过一次）。
bool RfidReader::selectCard(uint8_t i, MFRC522::Uid& uid) {
  if (i >= _n || !_pcs[i]) return false;
  MFRC522* r = _pcs[i];
  if (!r->PICC_IsNewCardPresent()) {
    byte atqa[2];
    byte atqaSize = sizeof(atqa);
    if (r->PICC_WakeupA(atqa, &atqaSize) != MFRC522::STATUS_OK) return false;
  }
  if (!r->PICC_ReadCardSerial()) return false;
  uid = r->uid;
  return true;
}

// SAK 识别：NTAG213/215/216 的 SAK 均为 0x00 → PICC_TYPE_MIFARE_UL；
// Classic 1K 为 0x08 → PICC_TYPE_MIFARE_1K
bool RfidReader::isUltralight(MFRC522* r) {
  MFRC522::PICC_Type t = r->PICC_GetType(r->uid.sak);
  return t == MFRC522::PICC_TYPE_MIFARE_UL;
}

bool RfidReader::readSlot(uint8_t i, uint8_t b4[16], uint8_t b5[16]) {
  if (i >= _n || !_pcs[i] || !ensurePresent(i)) return false;
  MFRC522* r = _pcs[i];
  MFRC522::Uid uid;
  if (!selectCard(i, uid)) {
    // 无卡和掉模块在这里 indistinguishable：连续失败满 4 轮才用探针仲裁，
    // 探针只读 VersionReg，不影响在用读卡器
    if (++_failStreak[i] >= 4) {
      _failStreak[i] = 0;
      if (_present[i] && !probeSlot(i)) {
        _present[i] = false;
        _retryMs[i] = millis();
        logLine("[RFID] slot " + String(i) + ": reader detached");
      }
    }
    return false;
  }
  _failStreak[i] = 0;

  uint8_t len = 18;
  uint8_t buf[18];
  MFRC522::StatusCode st;

  if (isUltralight(r)) {
    // NTAG213/215/216：页 4-7 即 b4，页 8-11 即 b5（MIFARE_Read 一次读 4 页=16B）
    st = (MFRC522::StatusCode)r->MIFARE_Read(4, buf, &len);
    if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); return false; }
    memcpy(b4, buf, 16);
    len = 18;
    st = (MFRC522::StatusCode)r->MIFARE_Read(8, buf, &len);
    r->PICC_HaltA();
    if (st != MFRC522::STATUS_OK) return false;
    memcpy(b5, buf, 16);
    return true;
  }

  MFRC522::MIFARE_Key key;
  for (uint8_t k = 0; k < 6; k++) key.keyByte[k] = 0xFF;

  st = (MFRC522::StatusCode)r->PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(r->uid));
  if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); return false; }

  st = (MFRC522::StatusCode)r->MIFARE_Read(4, buf, &len);
  if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); r->PCD_StopCrypto1(); return false; }
  memcpy(b4, buf, 16);

  len = 18;
  st = (MFRC522::StatusCode)r->MIFARE_Read(5, buf, &len);
  r->PICC_HaltA();
  r->PCD_StopCrypto1();
  if (st != MFRC522::STATUS_OK) return false;
  memcpy(b5, buf, 16);
  return true;
}

bool RfidReader::writeSlot(uint8_t i, const uint8_t b4[16], const uint8_t b5[16]) {
  if (i >= _n || !_pcs[i] || !ensurePresent(i)) {
    Serial.printf("[RFID] slot %u: no reader, write skipped\n", i);
    return false;
  }
  MFRC522* r = _pcs[i];
  MFRC522::Uid uid;
  if (!selectCard(i, uid)) {
    Serial.printf("[RFID] slot %u: no card (select failed)\n", i);
    return false;
  }
  Serial.printf("[RFID] slot %u: %s sak=0x%02X\n", i,
                isUltralight(r) ? "NTAG/UL" : "Classic",
                r->uid.sak);

  MFRC522::StatusCode st;

  if (isUltralight(r)) {
    // NTAG213/215/216：32B 拆成 8 页逐页写（页 4-11，三型号用户区都覆盖得到）
    uint8_t all[32];
    memcpy(all, b4, 16);
    memcpy(all + 16, b5, 16);
    for (uint8_t p = 0; p < 8; p++) {
      st = (MFRC522::StatusCode)r->MIFARE_Ultralight_Write(4 + p, all + p * 4, 4);
      if (st != MFRC522::STATUS_OK) {
        Serial.printf("[RFID] slot %u: UL write page %u failed: %s\n",
                      i, 4 + p, String(r->GetStatusCodeName(st)).c_str());
        r->PICC_HaltA();
        return false;
      }
    }
    // halt 前就地重读校验（免二次 select，避免“写上了却报失败”）
    uint8_t len = 18;
    uint8_t buf[18];
    uint8_t rb[32];
    st = (MFRC522::StatusCode)r->MIFARE_Read(4, buf, &len);
    if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); return false; }
    memcpy(rb, buf, 16);
    len = 18;
    st = (MFRC522::StatusCode)r->MIFARE_Read(8, buf, &len);
    r->PICC_HaltA();
    if (st != MFRC522::STATUS_OK) return false;
    memcpy(rb + 16, buf, 16);
    bool ok = memcmp(rb, all, 32) == 0;
    Serial.printf("[RFID] slot %u: UL write %s\n", i, ok ? "OK" : "verify mismatch");
    return ok;
  }

  MFRC522::MIFARE_Key key;
  for (uint8_t k = 0; k < 6; k++) key.keyByte[k] = 0xFF;

  st = (MFRC522::StatusCode)r->PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_B, 4, &key, &(r->uid));
  if (st != MFRC522::STATUS_OK) {
    Serial.printf("[RFID] slot %u: Classic auth B failed: %s\n",
                  i, String(r->GetStatusCodeName(st)).c_str());
    r->PICC_HaltA();
    return false;
  }

  st = (MFRC522::StatusCode)r->MIFARE_Write(4, (uint8_t*)b4, 16);
  if (st != MFRC522::STATUS_OK) {
    Serial.printf("[RFID] slot %u: Classic write B4 failed: %s\n",
                  i, String(r->GetStatusCodeName(st)).c_str());
    r->PICC_HaltA();
    r->PCD_StopCrypto1();
    return false;
  }
  st = (MFRC522::StatusCode)r->MIFARE_Write(5, (uint8_t*)b5, 16);
  if (st != MFRC522::STATUS_OK) {
    Serial.printf("[RFID] slot %u: Classic write B5 failed: %s\n",
                  i, String(r->GetStatusCodeName(st)).c_str());
    r->PICC_HaltA();
    r->PCD_StopCrypto1();
    return false;
  }

  // halt 前就地重读校验（免二次 select）
  uint8_t len = 18;
  uint8_t buf[18];
  uint8_t rb4[16], rb5[16];
  st = (MFRC522::StatusCode)r->MIFARE_Read(4, buf, &len);
  if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); r->PCD_StopCrypto1(); return false; }
  memcpy(rb4, buf, 16);
  len = 18;
  st = (MFRC522::StatusCode)r->MIFARE_Read(5, buf, &len);
  r->PICC_HaltA();
  r->PCD_StopCrypto1();
  if (st != MFRC522::STATUS_OK) return false;
  bool ok = memcmp(rb4, b4, 16) == 0 && memcmp(rb5, b5, 16) == 0;
  Serial.printf("[RFID] slot %u: Classic write %s\n", i, ok ? "OK" : "verify mismatch");
  return ok;
}
