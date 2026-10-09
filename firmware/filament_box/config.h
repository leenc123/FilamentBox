#pragma once
// config.h — FilamentBox 唯一配置入口（ESP32 30针 DevKit，Arduino 方式）

// ---- WiFi ----
// 出厂默认值；一旦用 AP 配网成功，凭据存 NVS，开机优先用 NVS 的，改这里不再生效
// （要换 WiFi：删 NVS 让设备重进配网，或重新配一次即可）
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-pass"

// ---- 拓竹打印机（局域网模式）----
// 出厂默认值；一旦在 /setup 页保存成功，以 NVS 为准，改这里不再生效
#define PRINTER_IP "192.168.1.100"   // 打印机屏幕 / 路由器里查
#define PRINTER_SERIAL "00M00A000000000"
#define PRINTER_ACCESS_CODE "12345678"  // 打印机屏幕 -> 设置 -> 局域网访问码
#define AMS_ID 0

// ---- 轮询 ----
#define POLL_MS 800        // 主循环间隔（毫秒）
#define STABLE_COUNT 2     // 连续读到相同才算有效（去抖）

// ---- VSPI 总线（4 路 RC522 共用）----
#define SPI_SCK_PIN 18
#define SPI_MOSI_PIN 23
#define SPI_MISO_PIN 19
#define RC522_RST_PIN 27   // 4 路共用 RST

// ---- 4 个槽位：tray_id 对应 AMS Lite 0-3（协议层，只能是 0-3），CS 独立（空闲高电平的安全脚）----
#define NUM_SLOTS 4
static const uint8_t SLOT_CS[NUM_SLOTS] = {5, 4, 13, 14};
static const uint8_t SLOT_TRAY[NUM_SLOTS] = {0, 1, 2, 3};
// 显示用槽号：用户界面一律从 1 开始（1-4，与 Bambu Studio 对齐）；
// 协议层（MQTT tray_id）仍用 SLOT_TRAY 的 0-3，两处不可混用
inline uint8_t trayDisplayNo(uint8_t i) { return (uint8_t)(SLOT_TRAY[i] + 1); }

// ---- SSD1306 OLED（硬件 I2C）----
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_ADDR 0x3C
// 顶部死区：屏幕上边坏掉的像素行数，坏区一个像素都不用（健康屏 0；坏顶屏改回 10，
// 同时把 miaoui/ui_conf.h 的 UI_PAGE_INIT_Y 改为 UI_FONT_HIGHT + 10）
#define OLED_TOP_DEAD 0
// 开机 IP 页停留毫秒数（WiFi 连上后显示本机 IP，然后自动切主页）
#define OLED_IP_SPLASH_MS 3000

// ---- 按键（输入上拉，接地为按下；MiaoUI 三键：上/下/确认）----
#define BUTTON_PIN 33       // 确认键（菜单确认 / 执行）
#define BTN_UP_PIN 32       // MiaoUI 上键（新增，接 GND）
#define BTN_DOWN_PIN 25     // MiaoUI 下键（新增，接 GND）
#define BTN_ENTER_PIN BUTTON_PIN  // MiaoUI 确认键复用旧按键

// ---- 板载状态灯（D2 = GPIO2 板载蓝灯，高电平亮）----
// GPIO2 是 strapping 脚：setup() 之前不驱动，reader.begin() 之后再 ledBegin() 即安全
#define LED_PIN 2
#define LED_ACTIVE_HIGH 1

// ---- 写卡 AP ----
#define SETUP_AP_SSID "FilamentBox-SETUP"
