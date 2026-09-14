// mqtt_push.cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "mqtt_push.h"

String buildAmsSetting(unsigned long seq, uint8_t amsId, uint8_t trayId,
                       const FilamentInfo& info, const String& colorRgba,
                       const String& trayType) {
  char buf[384];
  snprintf(buf, sizeof(buf),
      "{\"print\":{\"sequence_id\":\"%lu\",\"command\":\"ams_filament_setting\","
      "\"ams_id\":%u,\"tray_id\":%u,\"tray_info_idx\":\"%s\",\"tray_color\":\"%s\","
      "\"nozzle_temp_min\":%u,\"nozzle_temp_max\":%u,\"tray_type\":\"%s\"},"
      "\"user_id\":\"1234567890\"}",
      seq, amsId, trayId, info.idx, colorRgba.c_str(),
      info.nmin, info.nmax, trayType.c_str());
  return String(buf);
}

bool pushAmsSetting(const char* printerIp, const char* serial, const char* accessCode,
                    const String& payload, uint16_t port, bool useTls, String* errMsg) {
  Serial.print("[MQTT] port ");
  Serial.print(port);
  Serial.println(useTls ? " tls" : " plain");

  char topic[64];
  snprintf(topic, sizeof(topic), "device/%s/request", serial);

  bool ok;
  if (useTls) {
    WiFiClientSecure net;
    net.setInsecure();  // 打印机是自签名证书，局域网直连跳过校验
    net.setTimeout(5);
    PubSubClient mqtt(net);
    mqtt.setServer(printerIp, port);
    mqtt.setBufferSize(512);
    if (!mqtt.connect("filamentbox", "bblp", accessCode)) {
      if (errMsg) *errMsg = "mqtt connect fail st=" + String(mqtt.state());
      return false;
    }
    ok = mqtt.publish(topic, payload.c_str());
    mqtt.disconnect();
    net.stop();
  } else {
    WiFiClient net;  // 调试模式：明文 1883
    net.setTimeout(5);
    PubSubClient mqtt(net);
    mqtt.setServer(printerIp, port);
    mqtt.setBufferSize(512);
    if (!mqtt.connect("filamentbox", "bblp", accessCode)) {
      if (errMsg) *errMsg = "mqtt connect fail st=" + String(mqtt.state());
      return false;
    }
    ok = mqtt.publish(topic, payload.c_str());
    mqtt.disconnect();
    net.stop();
  }
  if (!ok && errMsg) *errMsg = "mqtt publish fail";
  return ok;
}
