// mqtt_push.cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "mqtt_push.h"
#include "log_ring.h"

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
  logLine("[MQTT] port " + String(port) + (useTls ? " tls" : " plain"));

  char topic[64];
  snprintf(topic, sizeof(topic), "device/%s/request", serial);

  bool ok;
  if (useTls) {
    WiFiClientSecure net;
    net.setInsecure();  // 打印机是自签名证书，局域网直连跳过校验
    net.setTimeout(5);
    PubSubClient mqtt(net);
    mqtt.setServer(printerIp, port);
    mqtt.setSocketTimeout(5);  // MQTT 握手 5 秒封顶；TCP 建连由下面的预建连 5 秒封顶，单次推送最坏约 10 秒
    mqtt.setKeepAlive(10);
    mqtt.setBufferSize(512);
    // 预建连（5 秒超时）：通则 PubSubClient 复用该连接只做 MQTT 握手；
    // 不通 5 秒即返，不再被无超时的 TCP 建连卡住约 30 秒
    if (!net.connected() && !net.connect(printerIp, port, 5000)) {
      if (errMsg) *errMsg = "tcp connect timeout";
      return false;
    }
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
    mqtt.setSocketTimeout(5);  // MQTT 握手 5 秒封顶；TCP 建连由下面的预建连 5 秒封顶
    mqtt.setKeepAlive(10);
    mqtt.setBufferSize(512);
    // 预建连（5 秒超时）：通则 PubSubClient 复用该连接只做 MQTT 握手；
    // 不通 5 秒即返，不再被无超时的 TCP 建连卡住约 30 秒
    if (!net.connected() && !net.connect(printerIp, port, 5000)) {
      if (errMsg) *errMsg = "tcp connect timeout";
      return false;
    }
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
