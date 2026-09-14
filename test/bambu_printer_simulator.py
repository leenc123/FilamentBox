#!/usr/bin/env python3
"""
FilamentBox 假打印机（PC 端，无真机联调用）
==========================================
模拟一台拓竹打印机（A1 Mini + AMS Lite）的 MQTT 侧：
  - 订阅 device/<serial>/request，接收 FilamentBox 的 ams_filament_setting
  - 更新 4 个 AMS 槽位并回发布 device/<serial>/report
  - 控制台可随时查看 4 槽对照表，确认 FilamentBox 的推送是否到达

只走明文 1883（对应固件 /setup 页的调试模式开关）。

使用方法：
    pip install paho-mqtt
    python test/run_mqtt_broker.py          # 终端 1：先起 broker（1883）
    python test/bambu_printer_simulator.py  # 终端 2：再起本模拟器
    # 常用参数：
    #   --serial TEST123456789   序列号（必须和 /setup 页填的一致）
    #   --broker 127.0.0.1       broker 地址
    #   --port 1883              broker 端口

联调拓扑（ESP32 与电脑须在同一 WiFi）：
    ESP32 --(1883 明文)--> 本机 broker <--  本模拟器
    ESP32 的 /setup 页：开调试模式，打印机 IP 填本机局域网 IP
"""

import argparse
import json
import socket
import time
import threading

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("需要安装 paho-mqtt: pip install paho-mqtt")
    exit(1)


def _get_lan_ip() -> str:
    """自动检测本机局域网 IP（填到 ESP32 的 /setup 页）"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
        finally:
            s.close()
        if ip and not ip.startswith("127."):
            return ip
    except Exception:
        pass
    return "127.0.0.1"


def _parse_args():
    parser = argparse.ArgumentParser(description="FilamentBox 假打印机")
    parser.add_argument("--broker", default="127.0.0.1", help="broker 地址")
    parser.add_argument("--port", type=int, default=1883, help="broker 端口")
    parser.add_argument("--serial", default="TEST123456789",
                        help="序列号（必须和 ESP32 /setup 页一致）")
    parser.add_argument("--ip", default=None, help="对外广告的 IP（默认自动检测）")
    args = parser.parse_args()
    return args.broker, args.port, args.serial, args.ip or _get_lan_ip()


BROKER_HOST, BROKER_PORT, SERIAL_NUMBER, LAN_IP = _parse_args()


def _blank_tray(tray_id: int, tray_type: str, color: str,
                idx: str, nmin: int, nmax: int) -> dict:
    """一个完整的 AMS 槽位（对齐 AMS Lite 0~3）"""
    return {
        "id": str(tray_id),
        "remain": 100,
        "k": 0.04,
        "n": 1,
        "cali_idx": -1,
        "tag_uid": "0000000000000000",
        "tray_id_name": "",
        "tray_info_idx": idx,
        "tray_type": tray_type,
        "tray_sub_brands": "",
        "tray_color": color,
        "tray_weight": "0",
        "tray_diameter": "1.75",
        "tray_temp": "0",
        "tray_time": "0",
        "bed_temp_type": "0",
        "bed_temp": "0",
        "nozzle_temp_max": str(nmax),
        "nozzle_temp_min": str(nmin),
        "xcam_info": "000000000000000000000000",
        "tray_uuid": "00000000000000000000000000000000",
        "ctype": 0,
        "cols": [color],
    }


class FakePrinter:
    def __init__(self):
        self.client = mqtt.Client(client_id=f"fakeprinter_{SERIAL_NUMBER}")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect

        self.connected = False
        self.sequence_id = 0
        self.trays = [
            _blank_tray(0, "PLA", "FF0000FF", "GFA00", 190, 230),
            _blank_tray(1, "PLA", "00FF00FF", "GFA00", 190, 230),
            _blank_tray(2, "PETG", "0000FFFF", "GFG00", 230, 260),
            _blank_tray(3, "ABS", "FFFFFFFF", "GFB00", 230, 270),
        ]

    # ---------- MQTT ----------
    def on_connect(self, client, userdata, flags, rc):
        print(f"[OK] 已连接到 broker {BROKER_HOST}:{BROKER_PORT}")
        self.connected = True
        topic = f"device/{SERIAL_NUMBER}/request"
        client.subscribe(topic)
        print(f"[OK] 已订阅: {topic}")
        threading.Thread(target=self.publish_loop, daemon=True).start()

    def on_disconnect(self, client, userdata, rc):
        print(f"[DISCONNECTED] rc={rc}")
        self.connected = False

    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except Exception as e:
            print(f"[ERR] 解析命令失败: {e}")
            return
        print(f"  <- 收到: {json.dumps(payload, ensure_ascii=False)[:200]}")
        self.handle_command(payload)

    def handle_command(self, payload):
        if "print" in payload:
            p = payload["print"]
            cmd = p.get("command")
            if cmd == "ams_filament_setting":
                self.handle_ams_setting(p)
            elif cmd in ("pushall",):
                self.publish_full_state()
            else:
                print(f"  -- 忽略不支持的命令: {cmd}")
        if "pushing" in payload:
            if payload["pushing"].get("command") == "pushall":
                self.publish_full_state()
        if "info" in payload:
            if payload["info"].get("command") == "get_version":
                self.publish_info()

    def handle_ams_setting(self, p):
        """核心：FilamentBox 的耗材更新指令，更新槽位并回发布"""
        try:
            tray_id = int(p.get("tray_id", -1))
            tray_type = str(p.get("tray_type", ""))
            color = str(p.get("tray_color", "")).upper()
            idx = str(p.get("tray_info_idx", ""))
            nmin = int(p.get("nozzle_temp_min", 0))
            nmax = int(p.get("nozzle_temp_max", 0))
        except (ValueError, TypeError) as e:
            print(f"  [AMS] 参数非法，拒绝: {e}")
            return
        if not 0 <= tray_id < len(self.trays):
            print(f"  [AMS] tray_id 越界: {tray_id}")
            return
        if len(color) != 8:
            print(f"  [AMS] 颜色非法: {color}")
            return
        t = self.trays[tray_id]
        t["tray_type"] = tray_type
        t["tray_color"] = color
        t["tray_info_idx"] = idx
        t["nozzle_temp_min"] = str(nmin)
        t["nozzle_temp_max"] = str(nmax)
        t["cols"] = [color]
        print(f"  [AMS] tray{tray_id} <- {tray_type} {color} ({idx} {nmin}-{nmax}C)")
        self.publish_full_state()

    # ---------- 发布 ----------
    def _report(self) -> dict:
        self.sequence_id += 1
        return {
            "print": {
                "gcode_state": "IDLE",
                "mc_percent": 0,
                "wifi_signal": "-45dBm",
                "ams": {
                    "ams": [{"id": "0", "humidity": "5", "temp": "26.5",
                             "tray": self.trays}],
                    "ams_exist_bits": "1",
                    "tray_exist_bits": "f",
                },
                "command": "push_status",
                "msg": 0,
                "sequence_id": str(self.sequence_id),
            }
        }

    def publish_full_state(self):
        topic = f"device/{SERIAL_NUMBER}/report"
        self.client.publish(topic, json.dumps(self._report()))

    def publish_info(self):
        topic = f"device/{SERIAL_NUMBER}/report"
        payload = {"info": {"command": "get_version", "sequence_id": "",
                            "result": "success", "reason": ""}}
        self.client.publish(topic, json.dumps(payload))

    def publish_loop(self):
        while self.connected:
            try:
                self.publish_full_state()
                time.sleep(5)
            except Exception as e:
                print(f"[ERR] 发布失败: {e}")
                time.sleep(1)

    # ---------- 控制台 ----------
    def print_ams_table(self):
        print("---- AMS 4 槽 ----")
        for t in self.trays:
            print(f"  tray{t['id']}: {t['tray_type']:10s} {t['tray_color']} "
                  f"({t['tray_info_idx']} {t['nozzle_temp_min']}-{t['nozzle_temp_max']}C)")
        print("-----------------")

    def run(self):
        print("=" * 50)
        print("FilamentBox 假打印机（A1 Mini + AMS Lite）")
        print("=" * 50)
        print(f"序列号: {SERIAL_NUMBER}")
        print(f"Broker: {BROKER_HOST}:{BROKER_PORT}（明文 1883）")
        print()
        print("ESP32 /setup 页这样填（电脑与 ESP32 须同一 WiFi）:")
        print(f"  打印机 IP: {LAN_IP}   <- 本机局域网 IP")
        print(f"  序列号:     {SERIAL_NUMBER}")
        print(f"  访问码:     任意（模拟器不校验）")
        print(f"  调试模式:   开（明文 1883）")
        print()
        print("刷卡后这里出现 [AMS] trayN <- TYPE COLOR 即联调通过")
        print("命令: ams（看 4 槽表）/ quit")
        print("=" * 50)
        self.print_ams_table()

        self.client.connect(BROKER_HOST, BROKER_PORT, 60)
        self.client.loop_start()

        try:
            while True:
                cmd = input().strip().lower()
                if cmd == "ams":
                    self.print_ams_table()
                elif cmd in ("quit", "exit"):
                    break
                elif cmd:
                    print("未知命令: ams / quit")
        except KeyboardInterrupt:
            pass

        self.client.loop_stop()
        self.client.disconnect()
        print("\n模拟器已停止")


if __name__ == "__main__":
    FakePrinter().run()
