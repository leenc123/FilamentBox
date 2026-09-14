#pragma once
// write_mode.h — AP 写卡模式：启动时按住按键超 3 秒进入
// 开放热点 FilamentBox-SETUP，浏览器 http://192.168.4.1/ 选槽/类型/颜色写卡
#include <Arduino.h>
#include "rfid_reader.h"

// 启动时检测：按键接地持续超阈值返回 true
bool shouldEnterWriteMode(uint8_t buttonPin, unsigned long holdMs);

// 进入 AP 写卡服务（阻塞运行，供 setup() 调用后直接 return）
void runWriteMode(RfidReader& reader);
