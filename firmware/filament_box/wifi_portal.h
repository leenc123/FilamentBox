#pragma once
// wifi_portal.h — WiFi 配网：凭据存 NVS，开机连不上自动开 AP 配网
// 配网 AP 与写卡 AP 同名（FilamentBox-SETUP）同 IP（192.168.4.1），但不会同时运行。
#include <Arduino.h>

class OledUi;

// 从 NVS 读已存 WiFi，无则回退 config.h 默认值
void loadWifiCreds(String& ssid, String& pass);
// 保存到 NVS（下次开机自动用）
void saveWifiCreds(const String& ssid, const String& pass);
// 用指定凭据连接（阻塞，最长 timeoutMs），OLED 显示 Connecting 进度；返回是否连上
bool connectWifiWith(const String& ssid, const String& pass,
                     unsigned long timeoutMs, OledUi* ui);
// 开机调用：连上直接返回；连不上则自动开 AP 配网（阻塞直到配网成功重启）
void ensureWifiOrPortal(OledUi* ui);
