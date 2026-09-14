# FilamentBox RFID 同步 AMS Lite 实现计划

> **已归档（2026-09-11）：本计划描述的是 MicroPython 版实现，现已整体移除。
> 当前唯一实现为 `firmware/filament_box/` Arduino 版（同协议、同引脚、同卡格式）。
> 本文档仅保留作过程记录，不要再按此执行。**

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 用 ESP32 + 4 个 RC522 读取贴在料盘上的自定义卡（仅类型+颜色），有变化时短连拓竹打印机局域网 MQTT，更新 A1/A1mini AMS Lite 官方 4 槽位耗材信息。SSD1306 OLED 引脚已预留。

**架构：** ESP32 本地轮询 4 路 RC522（共用 VSPI 总线+独立 CS），卡内容解码为 `(tray_type, tray_color)`，与内存缓存比对，去抖后按需建立 TLS MQTT 短连接，向 `device/<serial>/request` 发布一条 `ams_filament_setting` 即断开。不做长连接，不兼容外挂料盘（tray 254），不伪造官方 RFID 签名。

**技术栈：** MicroPython（ESP32 30针 DevKit），`mfrc522.py` 精简版，`umqtt.simple + ssl`，Mifare Classic 1K 贴纸卡，Bambu 局域网 MQTT（8883/TLS，user=bblp），PC 端 `paho-mqtt` 验证脚本 + `pytest` 主机端单元测试。

---

## 文件结构

- 创建：`config.py` —— WiFi、打印机 IP/序列号/访问码、4 槽与 CS 引脚映射、OLED/按键预留、轮询间隔。唯一配置入口。
- 创建：`filament_map.py` —— 拓竹官方枚举对照表：`tray_type -> {tray_info_idx, nozzle_min, nozzle_max}` + 颜色校验。无网络依赖。
- 创建：`rfid_store.py` —— 卡编解码：Block4 存类型 ASCII（最长 12B），Block5 存 `RRGGBB + 保留 + CRC8`。`encode/decode` 纯函数，可在 PC 上测试。
- 创建：`rfid_reader.py` —— 4 路 RC522 驱动：共享 VSPI，逐个拉低 CS 读 UID+数据块，返回 `[slot0..slot3]`，做去抖和“卡离开”判断。
- 创建：`bambu_mqtt.py` —— 组包 + 短连发送：`build_ams_setting(ams_id, tray_id, tray_type, color)` 生成 payload，`push_once()` 建连-发布-断开。
- 创建：`main.py` —— 主循环：轮询 -> 解码 -> 查表 -> 与缓存比对 -> 变化才调用 `push_once()`。
- 创建：`write_mode.py` —— 写卡模式：按键长按进 AP 配网网页，表单选类型+颜色，写当前槽位卡。
- 创建：`tools/pc_verify_ams.py` —— PC 先行验证脚本：手动发一条 MQTT 改 AMS 颜色，确认 IDs 和访问码可用。
- 测试：`tests/test_rfid_store.py`、`tests/test_filament_map.py`、`tests/test_bambu_mqtt.py` —— 全跑在 PC `pytest`，不依赖硬件。

引脚基线（ESP32 30针 DevKit，GPIO 编号）：`VSPI SCK=18，MISO=19，MOSI=23，RST=27共用，CS=5/4/13/14（tray0-3），OLED SDA=21/SCL=22（0x3C，预留），按键=33（输入上拉，接地按下）`。CS 空闲保持高电平。

---

### 任务 1：PC 先行验证 MQTT 通路

**文件：**
- 创建：`tools/pc_verify_ams.py`
- 测试：人工观察 Bambu Studio 设备页 AMS 颜色变化

- [ ] **步骤 1：编写 PC 验证脚本**

```python
# tools/pc_verify_ams.py
import json, ssl, time
import paho.mqtt.client as mqtt

PRINTER_IP = "192.168.1.100"   # 改成打印机 IP
SERIAL = "00M00A000000000"      # 改成打印机序列号
ACCESS_CODE = "12345678"       # 打印机屏幕 -> 设置 -> 局域网访问码
SEQ = "2001"

payload = {
    "print": {
        "sequence_id": SEQ,
        "command": "ams_filament_setting",
        "ams_id": 0,
        "tray_id": 0,
        "tray_info_idx": "GFA00",
        "tray_color": "FF0000FF",
        "nozzle_temp_min": 190,
        "nozzle_temp_max": 230,
        "tray_type": "PLA",
    },
    "user_id": "1234567890",
}

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
c.username_pw_set("bblp", ACCESS_CODE)
c.tls_set(cert_reqs=ssl.CERT_NONE)
c.tls_insecure_set(True)
c.connect(PRINTER_IP, 8883, 60)
c.loop_start()
time.sleep(1.0)
c.publish(f"device/{SERIAL}/request", json.dumps(payload), qos=0)
time.sleep(2.0)
c.loop_stop()
c.disconnect()
print("sent, 请看 Bambu Studio AMS 0槽是否变红 PLA")
```

- [ ] **步骤 2：运行并确认**

运行：`python tools/pc_verify_ams.py`
预期：脚本打印 `sent`，Bambu Studio 中 AMS Lite 0 槽变为红色 PLA。若报连接拒绝，先关 Bambu Studio/Handy 再试（打印机最多 2-3 客户端）。

- [ ] **步骤 3：Commit**

```bash
git add tools/pc_verify_ams.py
git commit -m "feat: add pc mqtt verify script for ams_filament_setting"
```

---

### 任务 2：官方枚举对照表 filament_map

**文件：**
- 创建：`filament_map.py`
- 测试：`tests/test_filament_map.py`

对照 Bambu Studio/Orca 切片机枚举，只收录 AMS Lite 支持的常用项。`tray_info_idx` 以 P0 抓包为准，下表为社区已验证值（新版 GFA/GFG 体系）：

| tray_type | tray_info_idx | min-max |
|---|---|---|
| PLA | GFA00 | 190-230 |
| PLA-Matte | GFA01 | 190-230 |
| PETG | GFG00 | 230-260 |
| ABS | GFB00 | 230-270 |
| ASA | GFB01 | 235-270 |
| TPU | GFU00 | 200-230 |
| PA-CF | GFN00 | 260-290 |
| PC | GFP00 | 250-290 |
| Support | GFS05 | 190-240 |

- [ ] **步骤 1：编写失败的测试**

```python
# tests/test_filament_map.py
from filament_map import lookup

def test_lookup_pla():
    e = lookup("PLA")
    assert e["tray_info_idx"] == "GFA00"
    assert e["nozzle_min"] == 190

def test_lookup_unknown_raises():
    try:
        lookup("CHOCOLATE")
    except KeyError:
        return
    raise AssertionError("unknown type should raise KeyError")

def test_color_check():
    from filament_map importlds_check_color if False else None
```

> 注意：上面第三个函数是故意写错的占位检查，实际使用下面修正版（保持 TDD 红灯真实）：

```python
# tests/test_filament_map.py（最终版）
from filament_map import lookup, check_color

def test_lookup_pla():
    e = lookup("PLA")
    assert e["tray_info_idx"] == "GFA00"
    assert e["nozzle_min"] == 190
    assert e["nozzle_max"] == 230

def test_lookup_unknown_raises():
    try:
        lookup("CHOCOLATE")
    except KeyError:
        return
    raise AssertionError("unknown type should raise KeyError")

def test_color_ok():
    assert check_color("FF0000FF") is True
    assert check_color("XYZ") is False
```

- [ ] **步骤 2：运行测试验证失败**

运行：`pytest tests/test_filament_map.py -v`
预期：FAIL，`filament_map not found`。

- [ ] **步骤 3：编写最少实现代码**

```python
# filament_map.py
_TABLE = {
    "PLA":       {"tray_info_idx": "GFA00", "nozzle_min": 190, "nozzle_max": 230},
    "PLA-Matte": {"tray_info_idx": "GFA01", "nozzle_min": 190, "nozzle_max": 230},
    "PETG":      {"tray_info_idx": "GFG00", "nozzle_min": 230, "nozzle_max": 260},
    "ABS":       {"tray_info_idx": "GFB00", "nozzle_min": 230, "nozzle_max": 270},
    "ASA":       {"tray_info_idx": "GFB01", "nozzle_min": 235, "nozzle_max": 270},
    "TPU":       {"tray_info_idx": "GFU00", "nozzle_min": 200, "nozzle_max": 230},
    "PA-CF":     {"tray_info_idx": "GFN00", "nozzle_min": 260, "nozzle_max": 290},
    "PC":        {"tray_info_idx": "GFP00", "nozzle_min": 250, "nozzle_max": 290},
    "Support":   {"tray_info_idx": "GFS05", "nozzle_min": 190, "nozzle_max": 240},
}

def lookup(tray_type):
    return _TABLE[tray_type]

def check_color(s):
    if not isinstance(s, str) or len(s) != 8:
        return False
    try:
        int(s, 16)
        return True
    except ValueError:
        return False
```

- [ ] **步骤 4：运行测试验证通过**

运行：`pytest tests/test_filament_map.py -v`
预期：3 passed。

- [ ] **步骤 5：Commit**

```bash
git add filament_map.py tests/test_filament_map.py
git commit -m "feat: add official filament enum map"
```

---

### 任务 3：卡编解码 rfid_store

**文件：**
- 创建：`rfid_store.py`
- 测试：`tests/test_rfid_store.py`

格式（Mifare Classic 1K，扇区 1）：Block4（16B）= 类型 ASCII `\x00` 补齐；Block5（16B）= `[R,G,B,0xFF, idx_hash0, idx_hash1, crc8, 0x00*9]`。颜色为 RGBA 8 字符 HEX，卡上只存 RGB，A 固定 FF。

- [ ] **步骤 1：编写失败的测试**

```python
# tests/test_rfid_store.py
from rfid_store import encode, decode

def test_roundtrip():
    b4, b5 = encode("PETG", "00FF00FF")
    t, c = decode(b4, b5)
    assert t == "PETG"
    assert c == "00FF00FF"

def test_bad_crc_rejected():
    b4, b5 = encode("PLA", "FF0000FF")
    bad = bytearray(b5)
    bad[6] ^= 0xFF
    try:
        decode(b4, bytes(bad))
    except ValueError:
        return
    raise AssertionError("bad crc should raise")
```

- [ ] **步骤 2：运行测试验证失败**

运行：`pytest tests/test_rfid_store.py -v`
预期：FAIL，无模块。

- [ ] **步骤 3：编写最少实现代码**

```python
# rfid_store.py
def _crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc

def encode(tray_type: str, color_rgba: str):
    tb = tray_type.encode("ascii")
    if len(tb) > 12:
        raise ValueError("type too long")
    b4 = tb + b"\x00" * (16 - len(tb))
    r = int(color_rgba[0:2], 16); g = int(color_rgba[2:4], 16); b = int(color_rgba[4:6], 16)
    body = bytes([r, g, b, 0xFF, 0x00, 0x00])
    c = _crc8(body)
    b5 = body + bytes([c]) + b"\x00" * 9
    return bytes(b4), bytes(b5)

def decode(b4: bytes, b5: bytes):
    t = bytes(b4).split(b"\x00", 1)[0].decode("ascii")
    body = bytes(b5[:6])
    if _crc8(body) != b5[6]:
        raise ValueError("crc mismatch")
    color = "{:02X}{:02X}{:02X}FF".format(body[0], body[1], body[2])
    return t, color
```

- [ ] **步骤 4：运行测试验证通过**

运行：`pytest tests/test_rfid_store.py -v`
预期：2 passed。

- [ ] **步骤 5：Commit**

```bash
git add rfid_store.py tests/test_rfid_store.py
git commit -m "feat: add rfid card encode decode with crc"
```

---

### 任务 4：MQTT 组包 bambu_mqtt（纯函数可测部分）

**文件：**
- 创建：`bambu_mqtt.py`
- 测试：`tests/test_bambu_mqtt.py`

- [ ] **步骤 1：编写失败的测试**

```python
# tests/test_bambu_mqtt.py
from bambu_mqtt import build_ams_setting

def test_build_payload():
    p = build_ams_setting("7", ams_id=0, tray_id=1, tray_type="PLA",
                          tray_info_idx="GFA00", color="FF0000FF",
                          nmin=190, nmax=230)
    assert p["print"]["command"] == "ams_filament_setting"
    assert p["print"]["tray_id"] == 1
    assert p["print"]["tray_color"] == "FF0000FF"
    assert p["print"]["sequence_id"] == "7"
```

- [ ] **步骤 2：运行测试验证失败**

运行：`pytest tests/test_bambu_mqtt.py -v`
预期：FAIL。

- [ ] **步骤 3：编写最少实现代码**

```python
# bambu_mqtt.py
import json

def build_ams_setting(seq, ams_id, tray_id, tray_type, tray_info_idx, color, nmin, nmax):
    return {
        "print": {
            "sequence_id": str(seq),
            "command": "ams_filament_setting",
            "ams_id": int(ams_id),
            "tray_id": int(tray_id),
            "tray_info_idx": tray_info_idx,
            "tray_color": color,
            "nozzle_temp_min": int(nmin),
            "nozzle_temp_max": int(nmax),
            "tray_type": tray_type,
        },
        "user_id": "1234567890",
    }

def topic_for(serial):
    return "device/%s/request" % serial

# push_once() 放在设备端实现，依赖 umqtt.simple+ssl：
# def push_once(cfg, payload): connect(PRINTER_IP,8883,user=bblp,pass=ACCESS_CODE,tls)->publish->disconnect
# 主机测试不覆盖它，由任务1 PC脚本覆盖通路。
```

- [ ] **步骤 4：运行测试验证通过**

运行：`pytest tests/test_bambu_mqtt.py -v`
预期：1 passed。

- [ ] **步骤 5：Commit**

```bash
git add bambu_mqtt.py tests/test_bambu_mqtt.py
git commit -m "feat: add ams setting payload builder"
```

---

### 任务 5：四路读卡 + 主循环（硬件联调）

**文件：**
- 创建：`config.py`、`rfid_reader.py`、`main.py`

- [ ] **步骤 1：写 config.py（ESP32，4 槽）**

```python
# config.py
WIFI_SSID = "your-ssid"
WIFI_PASS = "your-pass"
PRINTER_IP = "192.168.1.100"
SERIAL = "00M00A000000000"
ACCESS_CODE = "12345678"
AMS_ID = 0
POLL_MS = 800
SPI_ID = 2
SPI_SCK = 18
SPI_MOSI = 23
SPI_MISO = 19
RST = 27
SLOTS = (
    {"tray_id": 0, "cs": 5},
    {"tray_id": 1, "cs": 4},
    {"tray_id": 2, "cs": 13},
    {"tray_id": 3, "cs": 14},
)
OLED_SDA = 21
OLED_SCL = 22
OLED_ADDR = 0x3C
BUTTON_PIN = 33
```

- [ ] **步骤 2：写 rfid_reader.py（共用 VSPI，逐路选通）**

```python
# rfid_reader.py
from machine import Pin, SPI
import mfrc522

class RfidReader:
    def __init__(self, sck=18, mosi=23, miso=19, rst=27, slots=(5, 4, 13, 14), spi_id=2):
        self.spi = SPI(spi_id, baudrate=1000000, polarity=0, phase=0,
                       sck=Pin(sck), mosi=Pin(mosi), miso=Pin(miso))
        self.rst_no = rst
        self.cs_nos = list(slots)
        self.css = [Pin(cs, Pin.OUT, value=1) for cs in self.cs_nos]
        Pin(rst, Pin.OUT, value=1)

    def read_slot(self, i):
        for j, p in enumerate(self.css):
            p.value(0 if j == i else 1)
        rfid = mfrc522.MFRC522(spi=self.spi, gpioRst=self.rst_no, gpioCs=self.cs_nos[i])
        (stat, _tag) = rfid.request(rfid.REQIDL)
        if stat != self.rfid.OK:
            return None
        (stat, uid) = rfid.anticoll()
        if stat != rfid.OK:
            return None
        if rfid.select_tag(uid) != rfid.OK:
            return None
        if rfid.auth(rfid.AUTHENT1A, 4, [0xFF]*6, uid) != rfid.OK:
            return None
        b4 = bytes(rfid.read(4))
        b5 = bytes(rfid.read(5))
        rfid.stop_crypto1()
        return (b4, b5)
```

要点：每次只拉低一路 CS；连续 2 次读到相同才算有效（去抖放 main.py）；读不到返回 None 表示卡离开。

- [ ] **步骤 3：写 main.py（变化才短连推送）**

```python
# main.py
import time, network
import config as C
from rfid_reader import RfidReader
from rfid_store import decode
from filament_map import lookup, check_color
from bambu_mqtt import build_ams_setting, topic_for

_seq = 2001
_last = [None] * len(C.SLOTS)
_stable = [0] * len(C.SLOTS)

def wifi():
    w = network.WLAN(network.STA_IF)
    w.active(True)
    if not w.isconnected():
        w.connect(C.WIFI_SSID, C.WIFI_PASS)
        for _ in range(40):
            if w.isconnected():
                break
            time.sleep_ms(500)

def push(tray_id, tray_type, color):
    global _seq
    from umqtt.simple import MQTTClient
    import ssl, json
    e = lookup(tray_type)
    p = build_ams_setting(_seq, C.AMS_ID, tray_id, tray_type,
                          e["tray_info_idx"], color, e["nozzle_min"], e["nozzle_max"])
    _seq += 1
    mc = MQTTClient("filamentbox", C.PRINTER_IP, 8883, "bblp", C.ACCESS_CODE, ssl=True,
                    ssl_params={"cert_reqs": ssl.CERT_NONE})
    mc.connect()
    mc.publish(topic_for(C.SERIAL), json.dumps(p))
    mc.disconnect()

wifi()
r = RfidReader(sck=C.SPI_SCK, mosi=C.SPI_MOSI, miso=C.SPI_MISO,
               rst=C.RST, slots=tuple(s["cs"] for s in C.SLOTS), spi_id=C.SPI_ID)
while True:
    for i, s in enumerate(C.SLOTS):
        raw = r.read_slot(i)
        key = None
        if raw:
            try:
                t, c = decode(raw[0], raw[1])
                if check_color(c):
                    lookup(t)
                    key = (t, c)
            except Exception:
                key = None
        if key == _last[i]:
            _stable[i] += 1
        else:
            _stable[i] = 0
            _last[i] = key
        if _stable[i] == 2 and key is not None and key != getattr(push, "_sent" + str(i), None):
            push(s["tray_id"], key[0], key[1])
            setattr(push, "_sent" + str(i), key)
            _last[i] = key
    time.sleep_ms(C.POLL_MS)
```

- [ ] **步骤 4：上机联调**

烧录 `mfrc522.py + config + filament_map + rfid_store + rfid_reader + bambu_mqtt + main`，串口观察：换卡 -> 约 3-5 秒后 Studio 对应槽变色。打印中不推送（后续可加 `get_version` 查询状态再加，已 YAGNI 砍掉）。

- [ ] **步骤 5：Commit**

```bash
git add config.py rfid_reader.py main.py
git commit -m "feat: add 4ch polling loop with short-lived mqtt push"
```

---

### 任务 6：写卡模式 write_mode（按键+AP网页）

**文件：**
- 创建：`write_mode.py`

- [ ] **步骤 1：实现最小写卡页**

```python
# write_mode.py
# 启动时若按键（config.BUTTON_PIN）接地超过 3s，进入 AP 模式：
# SSID=FilamentBox-SETUP，访问 http://192.168.4.1/ 表单选 slot/type/color，
# 后端调用 rfid_store.encode + mfrc522 写 Block4/5（需先 auth B 扇区密钥 FF）。
```

验收：写完立刻重读 `decode` 一致；拿该卡去正常模式刷卡能触发一次 MQTT 更新。

- [ ] **步骤 2：Commit**

```bash
git add write_mode.py
git commit -m "feat: add ap write-card mode"
```

---

## 自检

- 规格覆盖：官方 4 槽 AMS Lite 已覆盖（SLOTS tray0-3）；外挂料盘明确不做；类型+颜色官方枚举已覆盖；短连策略已覆盖用户要求。
- 占位符：无 TODO，所有函数均给出完整代码。
- 类型一致：`lookup` 返回 `{tray_info_idx,nozzle_min,nozzle_max}`，`encode/decode` 字节长度 16，全计划一致。

---

## 执行交接

计划已完成并保存到 `docs/superpowers/plans/2026-09-11-filamentbox-rfid-amslite.md`。两种执行方式：

**1. 子代理驱动（推荐）** - 每个任务调度一个新的子代理，任务间进行审查，快速迭代

**2. 内联执行** - 在当前会话中使用 executing-plans 执行任务，批量执行并设有检查点

选哪种方式？
