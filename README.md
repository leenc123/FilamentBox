# FilamentBox — RFID 耗材识别箱体（ESP32 + 拓竹 AMS Lite）

给每个料盘贴一张可重写的 RFID 贴片卡（只存耗材类型 + 颜色），箱体识别到变化后，
通过同一局域网 MQTT 更新已绑定的拓竹打印机（A1 / A1mini）AMS Lite 官方 4 槽位的耗材信息。
不克隆拓竹官方 RFID（有 RSA 签名），走 `ams_filament_setting` 旁路指令；有变化才短连推送一次即断开。

项目简介见 [`项目简介.md`](项目简介.md)，详细实现计划见
[`docs/superpowers/plans/2026-09-11-filamentbox-rfid-amslite.md`](docs/superpowers/plans/2026-09-11-filamentbox-rfid-amslite.md)。

> 固件唯一实现：`firmware/filament_box/` Arduino 版（4 路读卡 + MQTT 短推 + OLED + AP 写卡），
> 直接用 Arduino IDE 烧录。`tools/pc_verify_ams.py` 为 PC 端 MQTT 验证脚本（烧板前先验证通路）。

---

## 一、Arduino 烧录

### 1.1 装环境

1. Arduino IDE 里添加 ESP32 支持：文件 → 首选项 → 附加开发板网址填
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`，
   开发板管理器搜 `esp32` 安装。
2. 库管理器安装四个库：`MFRC522`（GithubCommunity）、`PubSubClient`、`U8g2`、
   `WiFiManager`（tzapu 版，配网用）。
3. 开发板选 `ESP32 Dev Module`，Upload Speed `921600`，其余默认。

### 1.2 改配置烧录

1. 用 Arduino IDE 打开 `firmware/filament_box/filament_box.ino`（同目录其他文件自动进标签页）。
2. 先点"验证"编译通过，再只改 `config.h` 顶部：`WIFI_SSID / WIFI_PASS`（2.4G）、
   `PRINTER_IP / PRINTER_SERIAL / PRINTER_ACCESS_CODE`，点上传。
3. 串口监视器（115200）看到 `FilamentBox ready` 即成功。

### 1.4 首次开机配网（无网络自动进 AP，Captive Portal 自动弹出）

`config.h` 的 WiFi 只是出厂默认值。开机 15 秒连不上已存 WiFi（首次烧录就是这种情况），
设备自动开配网 AP（`FilamentBox-SETUP`，三方库 WiFiManager 接管，不再手写页面）：

1. 手机连 WiFi `FilamentBox-SETUP`（开放网络），配网页**自动弹出**；
   没弹出就手动打开 `http://192.168.4.1/`。
2. 下拉选你家 2.4G WiFi、输密码，点保存。设备试连成功后自动重启进正常模式。
3. 试连失败留在配网页，AP 不掉线，直接改完再提交。
4. 配网成功后凭据存 NVS，以后开机优先用它；要换 WiFi 就重新配一次。

> 配网 AP 名固定 `FilamentBox-SETUP` / `192.168.4.1`（无网络开机自动开，见 4.5 配网行）。

### 1.3 固件文件一览

`filament_box.ino`（主循环）+ `config.h`（出厂默认值）+
`filament_map.h`（官方枚举）+ `rfid_store.h`（卡编解码）+
`rfid_reader.h/.cpp`（4 路 RC522）+ `mqtt_push.h/.cpp`（组包 + 短连推送）+
`oled_ui.h/.cpp`（OLED 显示）+ `miaoui_menu.h/.cpp`（MiaoUI 菜单管理：喂数 + 写卡执行）+
`src/miaoui/`（MiaoUI 组件库 + 本机显示/按键对接层 `hal/`）+
`wifi_portal.h/.cpp`（开机配网）+ `printer_setup.h/.cpp`（打印机配置页）+
`web_dash.h/.cpp`（网页后台：管理首页 + 详细写卡）。

> 去抖规则：新卡需连续 3 次读到相同才推送（约 2.4 秒），变化才推，不重复推。

---

## 二、硬件清单

| 数量 | 物料 | 备注 / 购买关键词 |
|---|---|---|
| 1 | ESP32 30 针 DevKit（WROOM-32） | 运行主控，Arduino IDE 直接烧录 |
| 4 | RC522 读卡模块 | 对应 AMS Lite 4 个槽位 |
| 若干 | Mifare Classic 1K 贴纸卡 | 贴在线盘侧面，`Mifare 1K 不干胶标签` |
| 1 | SSD1306 OLED 0.96 寸 I2C 128x64 | 可选，不接也能跑推送 |
| 3 | 轻触按键 | 上/下/确认三键：MiaoUI 菜单导航，确认键兼写卡模式用 |
| 1 | 5V 转 3.3V Buck 模块（≥1A） | 给 4 个 RC522 + OLED 独立供电 |
| — | 杜邦线、面包板 / 洞洞板 | 共用 SPI 总线，走线尽量短 |

> RC522 只能吃 3.3V，接 5V 会烧。ESP32 开发板板载 3.3V 带不动 4 个读卡器，
> 必须用独立 Buck；Buck 与开发板共地。

---

## 三、接线图

### 3.1 总览（GPIO 编号，与 `config.h` 一致）

```text
                        ESP32 30针 DevKit
                   ┌─────────────────────────┐
                   │                         │
  VSPI SCK  GPIO18 │●                   ●│ 3V3 ──→ Buck 3.3V 输出 ──→ 各模块 3V3
  VSPI MISO GPIO19 │●                   ●│ GND ──→ 共地（Buck/RC522/OLED/按键）
  VSPI MOSI GPIO23 │●                   ●│ GPIO5  ──→ 第1槽 SDA(CS) [tray_id 0]
  共用 RST  GPIO27 │●                   ●│ GPIO4  ──→ 第2槽 SDA(CS) [tray_id 1]
  OLED SDA  GPIO21 │●                   ●│ GPIO13 ──→ 第3槽 SDA(CS) [tray_id 2]
  OLED SCL  GPIO22 │●                   ●│ GPIO14 ──→ 第4槽 SDA(CS) [tray_id 3]
   上键      GPIO32 │●──┐                ●│
   下键      GPIO25 │●──┤                ●│
   确认键    GPIO33 │●──┤ 三键另一脚 → GND  │
                    └─────────────────────────┘
```

### 3.2 每个 RC522 模块接线（4 个完全一样，只有 SDA 不同）

| RC522 脚 | 接到 | 说明 |
|---|---|---|
| SCK | GPIO18 | 4 个模块并联共用 |
| MOSI | GPIO23 | 4 个模块并联共用 |
| MISO | GPIO19 | 4 个模块并联共用 |
| RST | GPIO27 | 4 个模块并联共用 |
| SDA（即 CS） | GPIO5 / 4 / 13 / 14 | 每个模块独立一根，依次对应第 1–4 槽（协议 tray_id 0-3） |
| IRQ | 悬空 | 不用 |
| GND | GND（共地） | 与 Buck / ESP32 共地 |
| 3.3V | Buck 3.3V 输出 | 禁止接 5V / 禁止只靠开发板供电 |

### 3.3 OLED（可选）

| OLED 脚 | 接到 |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | Buck 3.3V |
| GND | 共地 |

地址固定 `0x3C`（见 `config.h` 的 `OLED_ADDR`）。不接 OLED 时固件自动无屏运行，推送不受影响。

### 3.4 三键（MiaoUI 菜单 + 写卡）

| 功能 | 引脚（`config.h`） | 接法 |
|---|---|---|
| 上 | `GPIO32`（`BTN_UP_PIN`） | 一脚接引脚，一脚接 `GND` |
| 下 | `GPIO25`（`BTN_DOWN_PIN`） | 同上 |
| 确认 | `GPIO33`（`BUTTON_PIN` / `BTN_ENTER_PIN`） | 同上 |

三键均为输入上拉，接地视为按下。菜单里上/下滚动、确认进入或执行；
写卡走 Home 的 `Write` 菜单（快捷）或网页 `/write`（全量），见 4.4。

---

## 四、联调流程

### 4.0 无真机：PC 假打印机联调（手上没打印机时用）

`test/` 里有一套假打印机：`run_mqtt_broker.py`（明文 broker，1883）+
`bambu_printer_simulator.py`（模拟 A1 Mini + AMS Lite，收 `ams_filament_setting`
并更新 4 个槽位）。全链路只走明文，对应固件 `/setup` 页的调试模式开关。

```bash
pip install -r requirements.txt   # paho-mqtt + amqtt
# 终端 1：先起 broker
python test/run_mqtt_broker.py --ws-port 0
# 终端 2：再起假打印机（记下它打印的本机局域网 IP）
python test/bambu_printer_simulator.py
```

然后 ESP32 的 `/setup` 页这样填：
- 打印机 IP：假打印机打印的本机局域网 IP（ESP32 与电脑须同一 WiFi）
- 序列号：`TEST123456789`（和模拟器默认一致）
- 访问码：任意（模拟器不校验）
- 调试模式：**开**（明文 1883）

刷卡后模拟器控制台出现 `[AMS] trayN <- TYPE COLOR` 即联调通过；
输入 `ams` 可随时打印当前 4 槽对照表。注意模拟器只认明文 1883，
切回真机记得把调试模式关掉。

### 4.1 PC 先验证 MQTT 通路（烧板前做，不接硬件也能做）

```bash
pip install paho-mqtt
# 先把 tools/pc_verify_ams.py 顶部的 PRINTER_IP / SERIAL / ACCESS_CODE 改成你的
python tools/pc_verify_ams.py
```

去 Bambu Studio 设备页看 AMS Lite 第 1 槽是否变成红色 PLA（验证脚本往 tray_id 0 推送）。
如果连接被拒，先关闭 Bambu Studio / Handy（打印机最多 2–3 个 MQTT 客户端），再试。
这步不通就不用往下烧板子。

### 4.2 打印机配置页（`/setup`，只管打印机三项）

`config.h` 的打印机 IP/序列号/访问码只是出厂默认值。设备正常联网后，
常驻配置页一直开着，同一 WiFi 下浏览器打开 `http://<本机IP>/setup` 即可修改
（本机 IP 看开机 IP 页 3 秒，或连续失败后的 OLED 提示屏，不用再猜地址）：

1. 填 IP、序列号、访问码，点保存。提交前固件先直连 `IP:8883`，
   通了才存 NVS，不通页面报错且旧配置继续用。
2. OLED 提示屏会直接给出本机 IP + `/setup` 路径（见 4.5），照着输就行。
3. 保存后下次换卡自动用新配置推送，无需重启。

> WiFi 只能在 AP 配网里改，`/setup` 不放 WiFi 项。

### 4.3 上电联调

1. 按第三章接好线，先不放卡，上电看串口：应无报错，先显示开机 IP 页（3 秒），
   然后切主页 `4 行槽位 + 第 5 行 FBX Ready`（顶部 10px 死区是黑的，属正常）。
2. 依次在每个槽位放已写好的卡：约 1–2 秒后 Bambu Studio 对应槽变色，
   OLED 该行同步显示类型和颜色，状态行显示 `SENT N 类型`（如 `SENT 1 PLA Matte`）。
3. 推送失败状态行显示 `FAIL N`，主循环不会死，可换卡重试。
4. 打印过程中不要换卡：此时推送可能失败且不会补推，打完把卡拿起重放一次即可。

### 4.4 写新卡（屏上快捷写卡 / 网页详细写卡）

屏上写卡（材料 6 家族 PLA/PETG/ABS/ASA/TPU/PA-CF × 品牌 0=Bambu官方/1=Generic通用 × 12 常用色；全量预设走网页 `/write`）：

1. Home 进 `写卡`：选槽位（1–4）/ 材料（`PLA/PETG/ABS/ASA/TPU/PA-CF`）/ 品牌（`Bambu`官方/`Generic`通用）/ 颜色（12 常用色中文名+HEX）。行名中文（点阵字库 `src/miaoui/fonts/font_cn12.c`，89字约2.3KB；加字先改 `tools/` 词表再跑 `tools/make_cn12_font.ps1` 重生成），点进去是选项列表，上/下循环选，确认返回；右侧数字列为序号；预设预览行带 `B/G` 前缀，颜色预览为英文短名+HEX；数字弹窗标题中文。
2. 把空白贴纸卡放到所选槽位的读卡器上，选 `-Write!` 确认，屏幕弹 `结果:` 显示中文结果（`成功 N` 即已自动推送验证；`写卡 N no card` 表示卡没放好，`no reader` 表示该槽读卡器未接入），约1秒后回写卡页。

网页详细写卡（全量 87 个 Bambu/Generic 预设 + 28 色块点选）：`/` 首页点"写卡"进 `/write`，流程同上（见 4.6）。

### 4.5 OLED 状态速查

| 屏幕显示（顶部 10px 死区弃用，全大字体 5 行） | 含义 |
|---|---|
| `正在连接` + 卡片/波纹 + 进度条 + `剩余 Ns秒` | 开机正在连已存 WiFi（15秒倒计时，波纹随进度变多） |
| `配网模式` + 卡片/呼吸波 + AP名 + IP + `已连接N` | 已自动开配网 AP，手机等自动弹出配网页（没弹出手动开 IP），按 1.4 配网；有人连上波纹加速 |
| `WiFi OK` + 本机IP + `/setup` | 开机 IP 页（停 3 秒），然后自动切主页 |
| `Printer cfg?` + 本机IP + `/setup save IP` | WiFi 通但连续 3 次推送失败，按屏上地址改打印机配置（见 4.2） |
| 槽位屏标题 `槽位 n/4` | 正常待机（n=有卡数）；行格式短名+中文色值（如 `PLA Matte 红色`），空槽显示 `空`，读卡器缺席显示 `空 !`，槽号 1–4 |
| 槽位屏标题 `成功/失败/写卡 N …` | N 槽推送成功/失败/写卡中，失败程序不死；约1秒后回 `槽位 n/4` |
| `网络重试`（槽位标题条） | 运行中掉线，自动重连；超 2 分钟连不上则重启回配网 |

### 4.6 网页后台（`/` 首页 + `/write` 写卡，不接按键也能用）

设备正常联网后，同一 WiFi 下浏览器打开 `http://<本机IP>/` 即管理首页：

1. 首页显示 4 个槽位色块 + 类型 + 状态行，整页每 5 秒自动刷新，换卡后颜色同步变化。
2. 点"写卡"进 `/write`：选槽位（1–4）/ 类型 / 颜色（类型为拓竹官方预设分组下拉、颜色为 28 色块点选，均不可自定义），把**空白贴纸卡放到所选槽位的读卡器上**再点 Write。
3. 写卡成功后设备自动推送一次到对应 AMS 槽验证，页面显示 `OK + SENT N`；卡已写入但推送失败会显示原因，设备稍后自动重试，无需重写。
4. `/setup` 保存成功页有"进入管理首页"链接；首页也有"打印机配置"入口，三个页面互通。

> 网页写卡和 AP 写卡（4.4）用同一套校验 + 写入逻辑，只是入口不同：
> 网页版不用重启、不切 AP；按键 + AP 版在无网络或调试时备用。
>
> 耗材枚举对齐 ha-bambulab 的 `filaments_detail.json`（约 100 个官方预设，卡内存预设 ID 如 `GFA01`，
> 推送 `tray_type` 用家族名如 `PLA`）。旧版已写好的卡（存 `PLA` / `PLA-Matte` 等）仍可识别，无需重写；
> 旧版 `PA-CF` 曾指向不存在的 `GFN00`，现自动归并到官方 `GFN03`。
>
> 槽位显示从 1 开始（1–4，与 Bambu Studio 对齐）；协议层 `tray_id` 仍为 0–3，`test/` 模拟器与验证脚本不受影响。

---

## 五、验收标准

- Arduino IDE 点"验证"编译通过，无报错。
- 换卡 → Studio 对应槽类型 + 颜色同步变化，OLED 同行一致。
- 卡拿走再放回相同卡 → 不重复推送（变化才推）。
- 不同槽各放各的卡 → 互不干扰。
- 断网 / 推送失败 → 状态行 `FAIL`，程序不死，恢复后自动继续；运行中掉线超 2 分钟自动重启回配网。
- 开机无网络 → 自动开配网 AP，OLED 显示配网信息（见 4.5）。
- 屏幕顶部 10px 死区弃用（黑边属正常）；开机先显示 IP 页 3 秒再切主页，待机第 5 行显示 `FBX Ready`（不再常驻 IP）。
- 打印机未配置/配错 → 连续 3 次推送失败后 OLED 给出本机 IP + `/setup`，改完自动恢复。
- 网页后台：`/` 首页四色块与实物一致（5 秒内刷新），`/write` 无按键完成写卡 + 自动推送验证。
- 屏上写卡：Home 进 `Write` 选槽/材料/品牌/颜色后确认，Slots 标题条 `SENT N` + 自动推送验证（见 4.4）。

## 六、故障排查

| 现象 | 查什么 |
|---|---|
| PC 验证脚本连不上 | IP/访问码是否正确；Studio/Handy 是否占着连接；打印机是否开了局域网模式 |
| 开机一直进配网 AP | WiFi 是 2.4G 吗；密码对吗；信号够强吗；NVS 里是旧密码就重配一次 |
| 一直弹打印机配置提示 | 按屏上 IP 打开 `/setup`，改对 IP/序列号/访问码；页面先报"不可达"说明 IP 或局域网模式有问题 |
| 板子一换卡就复位 | Buck 电流不够 / 3.3V 被拉垮；RC522 是否误接 5V |
| 某个槽永远读不到卡 | 该路 CS 线是否接错（5/4/13/14 对第 1–4 槽）；卡是否在感应距离内 |
| 槽位行 `空 !` | 该路读卡器缺席/被拔掉（拔线约3秒后显示，插回约2秒恢复；串口同步 `detached/attached`），热插拔无需重启 |
| OLED 黑屏但推送正常 | SDA/SCL 是否接反；串口看 `[OLED]` 日志：`no screen found`=线/电问题，`found at 0x3D`=已自动适配 |
| OLED 全程不亮 | 按附录 A 四步排查（串口日志 → 核线 → I2C 扫描 → 确认型号） |
| OLED 显示乱码 | 正常现象：OLED 只显示英文/数字，中文在网页端看；若英文也乱，检查是否 1.3 寸 SH1106 屏（需换驱动） |
| 配网页没自动弹出 | 手动打开 `http://192.168.4.1/`；安卓注意是否点了"保持连接"，iOS 等十几秒或开关一下 WiFi |
| 配网页 WiFi 列表为空 | 点 Scan/刷新重试；确认路由器开了 2.4G 且手机能搜到 |
| 屏上写卡报 `no card` | 空白卡是否放在所选槽位的读卡器上；是否 `no reader`（该槽模块未接入） |
| 屏上写卡报 `no reader` | 该槽 RC522 未接入或接线松动（共用 SPI + 独立 CS，见 3.2）；接上后约 2 秒自动识别 |
| IDE 验证报错缺库 | 四个库是否装全：`MFRC522` / `PubSubClient` / `U8g2` / `WiFiManager` |

---

## 附录 A：OLED 不亮四步排查

1. **看串口日志**（115200，按 RST）：`[OLED] found at 0x3C/0x3D`=屏和线没问题；
   `no screen found`=往下走；完全没输出=板子没跑起来（USB 线/驱动/供电问题）。
2. **核对四根线**：SDA→GPIO21、SCL→GPIO22（板上标 `D21/D22` 即是），VCC→3.3V，GND→共地。
   SDA/SCL 接反最常见。
3. **I2C 扫描**（隔离测试）：新建空白 sketch 只跑下面代码，串口看扫到谁——
   扫到 `0x3C/0x3D` 说明线没问题；什么都扫不到就量 OLED 的 VCC-GND 有没有 3.3V，
   或临时把 OLED 的 VCC 改接开发板 `3V3` 脚单测（OLED 仅约 20mA，板载带得动）。

```cpp
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("found at 0x"); Serial.println(a, HEX);
    }
  }
  Serial.println("scan done");
}
void loop() {}
```

4. **确认屏幕型号**：0.96 寸基本是 SSD1306，直接亮；**1.3 寸很多是 SH1106**，
   长得一样但 SSD1306 驱动点不亮，需要换构造函数，报型号即可改。
