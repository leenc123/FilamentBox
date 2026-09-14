// status_led.cpp — D2 板载 LED 非阻塞闪灯实现
#include "status_led.h"
#include "config.h"

#ifndef LED_PIN
#define LED_PIN 2
#endif
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif

static LedMode s_mode = LED_HEARTBEAT;
static unsigned long s_t0 = 0;
static bool s_level = false;

static inline void ledWrite(bool on) {
  if (on == s_level) return;
  s_level = on;
#if LED_ACTIVE_HIGH
  digitalWrite(LED_PIN, on ? HIGH : LOW);
#else
  digitalWrite(LED_PIN, on ? LOW : HIGH);
#endif
}

void ledBegin() {
  pinMode(LED_PIN, OUTPUT);
  s_level = false;
#if LED_ACTIVE_HIGH
  digitalWrite(LED_PIN, LOW);
#else
  digitalWrite(LED_PIN, HIGH);
#endif
  // 开机自检三闪：证明灯路是好的，之后进心跳
  for (uint8_t i = 0; i < 3; i++) {
#if LED_ACTIVE_HIGH
    digitalWrite(LED_PIN, HIGH);
#else
    digitalWrite(LED_PIN, LOW);
#endif
    delay(120);
#if LED_ACTIVE_HIGH
    digitalWrite(LED_PIN, LOW);
#else
    digitalWrite(LED_PIN, HIGH);
#endif
    delay(120);
  }
  s_level = false;
  s_mode = LED_HEARTBEAT;
  s_t0 = millis();
}

void ledSet(LedMode m) {
  if (m == LED_SUCCESS_PULSE) {  // 脉冲每次都从头播
    s_mode = m;
    s_t0 = millis();
    return;
  }
  if (m == s_mode) return;
  s_mode = m;
  s_t0 = millis();
}

void ledSuccessPulse() {
  ledSet(LED_SUCCESS_PULSE);
}

LedMode ledCurrent() { return s_mode; }

void ledTick() {
  unsigned long t = millis() - s_t0;
  switch (s_mode) {
    case LED_HEARTBEAT: {
      // 周期 2000ms 双脉冲：0-120亮，120-240灭，240-360亮，其余灭
      bool on = (t < 120) || (t >= 240 && t < 360);
      ledWrite(on);
      if (t >= 2000) s_t0 = millis();
      break;
    }
    case LED_WIFI_CONNECT:
      // 500ms 慢闪
      ledWrite((t / 500) % 2 == 0);
      if (t >= 60000) s_t0 = millis();  // 防 millis 溢出前计时器过大
      break;
    case LED_AP_PORTAL: {
      // 双闪：100亮-100灭-100亮-700灭，周期 1000ms
      bool on = (t < 100) || (t >= 200 && t < 300);
      ledWrite(on);
      if (t >= 1000) s_t0 = millis();
      break;
    }
    case LED_SUCCESS_PULSE: {
      // 快闪 3 下：120ms 间隔，共 720ms，播完回 HEARTBEAT
      if (t >= 720) {
        s_mode = LED_HEARTBEAT;
        s_t0 = millis();
        ledWrite(false);
      } else {
        ledWrite((t / 120) % 2 == 0);
      }
      break;
    }
    case LED_ERROR:
      // 200ms 快闪告警
      ledWrite((t / 200) % 2 == 0);
      if (t >= 60000) s_t0 = millis();
      break;
  }
}
