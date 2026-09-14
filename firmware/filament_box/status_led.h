#pragma once
// status_led.h — D2 板载 LED 状态灯（非阻塞，millis 驱动）
// 心跳待机 + 慢闪连网 + 双闪AP + 成功脉冲 + 失败快闪
#include <Arduino.h>
#include <stdint.h>

enum LedMode : uint8_t {
  LED_HEARTBEAT,      // 正常待机：每 2s 双脉冲（亮120-灭120-亮120），显眼但不刺眼
  LED_WIFI_CONNECT,   // 连 WiFi / 掉线重连：500ms 慢闪
  LED_AP_PORTAL,      // 配网 AP / 写卡 AP：双闪
  LED_SUCCESS_PULSE,  // 推送成功：快闪 3 下后自动回 HEARTBEAT
  LED_ERROR           // 推送失败 / 需改配置：200ms 快闪
};

void ledBegin();            // 初始化 GPIO（setup 中 reader.begin() 之后调用）
void ledSet(LedMode m);     // 切换模式（同模式重复调用不重启计时，SUCCESS 除外）
void ledTick();             // 每个 loop 及阻塞循环中调用一次
void ledSuccessPulse();     // SENT 时调用一次，播完自动回 HEARTBEAT
LedMode ledCurrent();
