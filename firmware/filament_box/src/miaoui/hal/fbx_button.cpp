// fbx_button.cpp — 三键扫描实现（Arduino C++，50ms 消抖；语义直返 UI_ACTION 编码）
#include <Arduino.h>
#include "fbx_button.h"
#include "../../../config.h"

#ifndef BTN_UP_PIN
#define BTN_UP_PIN 32
#endif
#ifndef BTN_DOWN_PIN
#define BTN_DOWN_PIN 25
#endif
#ifndef BTN_ENTER_PIN
#define BTN_ENTER_PIN 33
#endif

#define FBXBTN_NONE 0
#define FBXBTN_UP 1
#define FBXBTN_DOWN 2
#define FBXBTN_ENTER 3
#define FBXBTN_DEBOUNCE_MS 50

void fbxButtonInit(void) {
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_ENTER_PIN, INPUT_PULLUP);
}

uint8_t fbxKeyScan(void) {
  static uint8_t lastState = FBXBTN_NONE;
  static unsigned long lastChange = 0;

  uint8_t cur = FBXBTN_NONE;
  if (digitalRead(BTN_UP_PIN) == LOW) {
    cur = FBXBTN_UP;
  } else if (digitalRead(BTN_DOWN_PIN) == LOW) {
    cur = FBXBTN_DOWN;
  } else if (digitalRead(BTN_ENTER_PIN) == LOW) {
    cur = FBXBTN_ENTER;
  }

  if (cur != lastState) {
    lastChange = millis();
    lastState = cur;
  }
  if (millis() - lastChange > FBXBTN_DEBOUNCE_MS) {
    return cur;
  }
  return FBXBTN_NONE;
}
