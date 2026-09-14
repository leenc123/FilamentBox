#pragma once
// printer_setup.h — 打印机配置：NVS 存 IP/序列号/访问码，常驻 /setup 页
// 只管打印机三项（WiFi 只在 AP 配网改）。连续推送失败 3 次则 OLED 提示
// 打开 http://<本机IP>/setup 修改，保存前先做 8883 可达性检测。
#include <Arduino.h>

class OledUi;
class WebServer;

#define PRINTER_HINT_STREAK 3  // 连续推送失败达此次数弹 OLED 提示（WiFi 正常时）

struct PrinterCfg {
  String ip;
  String serial;
  String code;
  bool dbg = false;  // 调试模式：true 走明文 1883（仅排查用），false 走 8883/TLS
  uint16_t mqttPort() const { return dbg ? 1883 : 8883; }
};

// 从 NVS 读打印机配置，无则回退 config.h 默认值
void loadPrinterCfg(PrinterCfg& cfg);
// 保存到 NVS（下次开机自动用）
void savePrinterCfg(const PrinterCfg& cfg);
// TCP 直连 ip:port（最长 timeoutMs），通返回 true（可达性检测用，不发 MQTT）
bool checkPrinterReachable(const String& ip, uint16_t port,
                           unsigned long timeoutMs = 5000);
// 启动常驻配置服务（正常模式调用一次；每次 loop 调 printerSetupHandle）
void startPrinterSetup(PrinterCfg& cfg);
// 每次 loop 调用（处理配置服务请求，开销可忽略）
void printerSetupHandle();
// 取常驻 Web 服务实例（给管理首页 /、写卡 /write 等路由复用同一 80 端口）
WebServer& setupWebServer();
// 连续失败达阈值时在 OLED 显示提示屏（含本机 IP + /setup 路径）
void showPrinterHint(OledUi* ui);
