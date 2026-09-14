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
