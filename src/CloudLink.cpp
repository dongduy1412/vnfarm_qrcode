#include "CloudLink.h"
#include "CloudRootCA.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <time.h>

#ifndef QR_CLOUD_BASE_URL
#define QR_CLOUD_BASE_URL "https://vnfarm-qrcode-device-link.dong141220047.workers.dev"
#endif

namespace {
struct Message {
  char body[640];
  char device[13];
  char nonce[33];
};
QueueHandle_t messages = nullptr;
uint32_t lastAttempt = 0;
bool attempted = false;

// Bounded HTTP output sink; HTTPClient handles chunked transfer decoding.
class ResponseBuffer : public Stream {
public:
  char data[768] = {};
  size_t length = 0;
  size_t write(uint8_t byte) override {
    if (length >= sizeof(data) - 1) return 0;
    data[length++] = static_cast<char>(byte);
    return 1;
  }
  size_t write(const uint8_t* bytes, size_t count) override {
    size_t remaining = sizeof(data) - 1 - length;
    if (count > remaining) return 0;
    memcpy(data + length, bytes, count);
    length += count;
    return count;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

void sendTask(void*) {
  Message message;
  while (true) {
    if (xQueueReceive(messages, &message, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) continue;
    WiFiClientSecure tls;
    tls.setCACert(CLOUD_ROOT_CA);
    tls.setHandshakeTimeout(5);
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(4000);
    http.setReuse(false);
    http.setUserAgent("Mozilla/5.0 (compatible; VNFARM-ESP32/0.1.9)");
    String url = String(QR_CLOUD_BASE_URL) + "/api/devices/" + message.device + "/ping";
    if (!http.begin(tls, url)) {
      Serial.println("DO link: HTTP initialization failed");
      continue;
    }
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(reinterpret_cast<uint8_t*>(message.body), strlen(message.body));
    bool received = false;
    if (code == 200) {
      ResponseBuffer buffer;
      int copied = http.writeToStream(&buffer);
      StaticJsonDocument<768> response;
      if (copied > 0 && !deserializeJson(response, buffer.data, buffer.length) &&
          response["type"] == "pong" && response["device_id"] == message.device &&
          response["nonce"] == message.nonce && response["sequence"].is<uint32_t>()) {
        Serial.printf("DO pong verified: sequence=%lu server_time=%lu\n",
          response["sequence"].as<unsigned long>(), response["server_time"].as<unsigned long>());
        received = true;
      }
    }
    if (!received) Serial.printf("DO link failed: HTTP %d\n", code);
    http.end();
  }
}
}

bool CloudLink::begin() {
  if (messages) return true;
  messages = xQueueCreate(1, sizeof(Message));
  if (!messages) return false;
  if (xTaskCreate(sendTask, "cloud-link", 12288, nullptr, 1, nullptr) != pdPASS) {
    vQueueDelete(messages);
    messages = nullptr;
    return false;
  }
  Serial.printf("DO link endpoint: %s\n", QR_CLOUD_BASE_URL);
  return true;
}

void CloudLink::tick(DeviceKeys& keys, const String& deviceId) {
  if (!messages || WiFi.status() != WL_CONNECTED || time(nullptr) < 1704067200) return;
  if (attempted && static_cast<uint32_t>(millis() - lastAttempt) < 30000) return;
  attempted = true;
  lastAttempt = millis();
  uint8_t random[16];
  esp_fill_random(random, sizeof(random));
  char nonce[33];
  for (size_t i = 0; i < sizeof(random); ++i) snprintf(nonce + i * 2, 3, "%02x", random[i]);
  uint32_t now = static_cast<uint32_t>(time(nullptr));
  uint32_t uptime = millis() / 1000;
  String payload = "ping-v1\n" + deviceId + "\n" + keys.keyId() + "\n" + String(now) + "\n" + nonce + "\n" + String(uptime);
  String signature;
  // Key access remains on the Arduino loop task, alongside QR signing.
  if (!keys.sign(payload, signature)) {
    Serial.println("DO link: could not sign ping");
    return;
  }
  StaticJsonDocument<640> json;
  json["device_id"] = deviceId;
  json["kid"] = keys.keyId();
  json["iat"] = now;
  json["nonce"] = nonce;
  json["uptime"] = uptime;
  json["sig"] = signature;
  Message message = {};
  if (json.overflowed() || measureJson(json) >= sizeof(message.body)) return;
  serializeJson(json, message.body, sizeof(message.body));
  deviceId.toCharArray(message.device, sizeof(message.device));
  memcpy(message.nonce, nonce, sizeof(nonce));
  if (xQueueSend(messages, &message, 0) != pdTRUE) Serial.println("DO link: previous ping still pending");
}
