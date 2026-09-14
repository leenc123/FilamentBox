// rfid_reader.cpp
#include <SPI.h>
#include "rfid_reader.h"

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
    _pcs[i] = new MFRC522(_cs[i], _rst);
    _pcs[i]->PCD_Init();
  }
}

// 寻卡 + 防冲突 + 选中，成功输出 uid
bool RfidReader::selectCard(uint8_t i, MFRC522::Uid& uid) {
  if (i >= _n || !_pcs[i]) return false;
  MFRC522* r = _pcs[i];
  if (!r->PICC_IsNewCardPresent()) return false;
  if (!r->PICC_ReadCardSerial()) return false;
  uid = r->uid;
  return true;
}

bool RfidReader::readSlot(uint8_t i, uint8_t b4[16], uint8_t b5[16]) {
  if (i >= _n || !_pcs[i]) return false;
  MFRC522* r = _pcs[i];
  MFRC522::Uid uid;
  if (!selectCard(i, uid)) return false;

  MFRC522::MIFARE_Key key;
  for (uint8_t k = 0; k < 6; k++) key.keyByte[k] = 0xFF;

  uint8_t len = 18;
  uint8_t buf[18];
  MFRC522::StatusCode st;

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
  if (i >= _n || !_pcs[i]) return false;
  MFRC522* r = _pcs[i];
  MFRC522::Uid uid;
  if (!selectCard(i, uid)) return false;

  MFRC522::MIFARE_Key key;
  for (uint8_t k = 0; k < 6; k++) key.keyByte[k] = 0xFF;

  MFRC522::StatusCode st = (MFRC522::StatusCode)r->PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_B, 4, &key, &(r->uid));
  if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); return false; }

  st = (MFRC522::StatusCode)r->MIFARE_Write(4, (uint8_t*)b4, 16);
  if (st != MFRC522::STATUS_OK) { r->PICC_HaltA(); r->PCD_StopCrypto1(); return false; }
  st = (MFRC522::StatusCode)r->MIFARE_Write(5, (uint8_t*)b5, 16);
  r->PICC_HaltA();
  r->PCD_StopCrypto1();
  if (st != MFRC522::STATUS_OK) return false;

  // 写完重读校验
  uint8_t rb4[16], rb5[16];
  if (!readSlot(i, rb4, rb5)) return false;
  return memcmp(rb4, b4, 16) == 0 && memcmp(rb5, b5, 16) == 0;
}
