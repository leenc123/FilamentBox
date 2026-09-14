#pragma once
// mqtt_push.h — 拓竹局域网 MQTT 短连推送（连上-发一条-断开，不做长连接）
#include <Arduino.h>
#include <stdint.h>
#include "filament_map.h"

// 组包：生成 ams_filament_setting 的 JSON（与 Python 版 build_ams_setting 同字段）
String buildAmsSetting(unsigned long seq, uint8_t amsId, uint8_t trayId,
                       const FilamentInfo& info, const String& colorRgba,
                       const String& trayType);

// 短连推送一次：成功返回 true。port/useTls 由 PrinterCfg.mqttPort()/dbg 决定
// （8883/TLS 正常模式，1883/明文仅调试）。失败原因写 errMsg（可为 nullptr）
bool pushAmsSetting(const char* printerIp, const char* serial, const char* accessCode,
                    const String& payload, uint16_t port, bool useTls,
                    String* errMsg = nullptr);
