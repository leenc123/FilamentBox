# AGENTS.md — FilamentBox 协作约定

## 编译上传（硬性规定）

- Agent **禁止**自行编译或上传固件：不运行 `arduino-cli compile / upload`，
  也不装工具链做等价本地编译。
- Agent 改完代码后只做只读静态检查（如 include 路径、符号引用核对），
  然后告知用户，由用户自己在 **Arduino IDE 客户端点"验证 / 上传"**。
- 用户反馈编译错误时，Agent 根据 IDE 返回的报错信息修代码，修完仍由用户编译。

## Arduino String 拼接（硬性规定，已踩坑两次）

- ESP32 core 的 `String` 只保证**字面量开头**的链式 `+` 可编译：
  ✅ `"abc" + String(x) + "def"`（字面量开头，后面随便接 `String` / 字面量）
  ❌ `String("abc") + String(x)`、`String(a) + String(b) + "c"`（`String` 开头的链会报
  `conversion from 'int' to 'const String' is ambiguous`）
- 新增/改写任何 `String` 拼接时必须字面量开头；全树复查可用：
  `rg 'String\("[^"]*"\) \+' firmware/filament_box --glob '!libraries'`（期望零命中）。
- 多参函数（如 `portalFrame` 8 参）改签名后，必须逐个重数调用点实参个数；
  有缺省值也要从**尾部整段省略**，禁止隔位传参——`portalFrame(ui,0,2,t,l1,"",-1)` 里
  `-1` 会绑到 `l3`（`const String&`）而不是 `fracBar`，照样报这个错（2026-10 实锤）；
  正确写法是 8 个全传，或省略末尾（`..., "", "")` 靠缺省 `fracBar=-1`）。
- 用户只贴一行报错看不出位置时，先让用户贴带 `file:line` 的完整报错。

## 构建背景（备注）

- 固件：`firmware/filament_box/`，Arduino sketch，主文件与同名 `.ino` 一致。
- 开发板：`ESP32 Dev Module`（FQBN `esp32:esp32:esp32`），Upload Speed `921600`。
- 依赖库（IDE 库管理器安装）：`MFRC522` / `PubSubClient` / `U8g2` / `WiFiManager`（tzapu）。
- Arduino IDE 只编译 sketch 根目录和 `src/`（递归），**自定义子目录会被忽略**：
  MiaoUI 源码必须放在 `src/miaoui/` 下， sketch 根的 `miaoui/` 不会被编译。
- 程序空间占用已约 91%（Default 分区 1.28MB app 区）；再膨胀就换 `Minimal SPIFFS` 分区。
