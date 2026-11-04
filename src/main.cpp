#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <time.h>
#include "AppConfig.h"
#include "DeviceKeys.h"
#include "Screen.h"
#include "CloudLink.h"

namespace {
DeviceKeys keys;
WebServer portal(80);
DNSServer portalDns;
String wifiSsid, wifiPassword, checkinUrl, deviceId;
uint32_t qrSeconds = DEFAULT_QR_SECONDS;
bool provisioning = false;
bool initialized = false;
bool restartPending = false;
bool timeReady = false;
uint32_t restartAt = 0;
uint32_t lastWifiAttempt = 0;
time_t displayedSlot = 0;
String displayedKid;
String usbCommand;

// Local USB maintenance only. Keep the signing key and saved settings;
// mark setup incomplete so the portal also survives a power cycle.
void handleUsbSetup() {
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (usbCommand.length() < 32) usbCommand += ch;
      else usbCommand = "INVALID";
      continue;
    }
    if (usbCommand == "SETUP_WIFI" && !provisioning) {
      Preferences store;
      bool ok = store.begin("qr-config", false);
      if (ok) {
        ok = store.putBool("configured", false) == 1;
        store.end();
      }
      if (ok) {
        Serial.println("Wi-Fi setup requested via USB; signing key preserved. Restarting.");
        Serial.flush();
        delay(100);
        ESP.restart();
      } else Serial.println("Could not enable Wi-Fi setup.");
    }
    usbCommand = "";
  }
}

String htmlEscape(String input) {
  input.replace("&", "&amp;");
  input.replace("<", "&lt;");
  input.replace(">", "&gt;");
  input.replace("\"", "&quot;");
  input.replace("'", "&#39;");
  return input;
}

bool validUrl(const String& url) {
  // Signed query fields go before an optional hash-router fragment (e.g. #/).
  if (url.length() == 0) return true; // Setup can precede backend availability.
  if (url.length() > MAX_CHECKIN_URL_LENGTH) return false;
  int fragmentAt = url.indexOf('#');
  String address = fragmentAt >= 0 ? url.substring(0, fragmentAt) : url;
  if (fragmentAt >= 0 && (!url.substring(fragmentAt + 1).startsWith("/") ||
      url.indexOf('#', fragmentAt + 1) >= 0)) return false;
  bool https = address.startsWith("https://") && address.length() > 8;
  bool localHttp = false;
#if QR_ALLOW_HTTP_TEST
  if (address.startsWith("http://")) {
    String host = address.substring(7);
    int slash = host.indexOf('/');
    if (slash >= 0) host = host.substring(0, slash);
    int colon = host.indexOf(':');
    if (colon >= 0) host = host.substring(0, colon);
    IPAddress ip;
    if (ip.fromString(host)) {
      localHttp = ip[0] == 10 || (ip[0] == 192 && ip[1] == 168) ||
          (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31);
    }
  }
#endif
  if ((!https && !localHttp) || url.indexOf('?') >= 0 || url.indexOf('@') >= 0) return false;
  for (size_t i = 0; i < url.length(); ++i)
    if (url[i] <= 32 || url[i] >= 127) return false;
  return true;
}

void showPublicKey() {
  portal.sendHeader("Cache-Control", "no-store");
  portal.send(200, "application/x-pem-file", keys.publicPem());
}

void saveSetup() {
  String ssid = portal.arg("ssid");
  String password = portal.arg("password");
  String url = portal.arg("url");
  String secondsText = portal.arg("seconds");
  ssid.trim();
  url.trim();
  bool digitsOnly = secondsText.length() > 0 && secondsText.length() <= 2;
  for (size_t i = 0; i < secondsText.length(); ++i)
    if (secondsText[i] < '0' || secondsText[i] > '9') digitsOnly = false;
  long seconds = secondsText.toInt();
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
      (!password.isEmpty() && password.length() < 8) || !validUrl(url) ||
      !digitsOnly || seconds < 10 || seconds > 60) {
    portal.send(400, "text/plain; charset=utf-8", "Thong tin khong hop le. Chu ky 10-60 giay; kiem tra URL va Wi-Fi.");
    return;
  }
  // Do not commit credentials before testing them, so a typo cannot lock setup.
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    portal.send(400, "text/plain; charset=utf-8", "Khong ket noi duoc Wi-Fi. Kiem tra ten/mat khau va thu lai.");
    return;
  }
  Preferences store;
  if (!store.begin("qr-config", false)) {
    portal.send(500, "text/plain", "Khong mo duoc bo nho cau hinh.");
    return;
  }
  bool ok = store.putBool("configured", false) == 1;
  ok = ok && store.putString("ssid", ssid) == ssid.length() &&
      store.putString("pass", password) == password.length() &&
      store.putString("url", url) == url.length() &&
      store.putUInt("interval", seconds) == sizeof(uint32_t);
  ok = ok && store.getString("ssid", "__missing") == ssid &&
      store.getString("pass", "__missing") == password &&
      store.getString("url", "__missing") == url && store.getUInt("interval", 0) == seconds;
  if (ok) ok = store.putBool("configured", true) == 1;
  store.end();
  if (!ok) {
    portal.send(500, "text/plain", "Luu cau hinh that bai. Chua danh dau thiet lap thanh cong.");
    return;
  }
  portal.send(200, "text/plain; charset=utf-8", "Da luu. Thiet bi se khoi dong lai. Hay luu public key vao server truoc khi choi.");
  restartPending = true;
  restartAt = millis() + 2000;
}

bool startPortal() {
  WiFi.mode(WIFI_AP_STA);
  // Avoid saving another copy of credentials outside our config namespace.
  WiFi.persistent(false);
  String apName = "VNFARM-" + deviceId.substring(deviceId.length() - 6);
  if (!WiFi.softAP(apName.c_str(), SETUP_WIFI_PASSWORD)) return false;
  if (!portalDns.start(53, "*", WiFi.softAPIP())) return false;
  // RF is enabled before creating a signing key, to supply hardware RNG entropy.
  if (!keys.begin()) return false;
  portal.on("/", HTTP_GET, []() {
    String page = "<!doctype html><html lang='vi'><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
      "<title>Thiết lập thiết bị</title><body><h1>Thiết lập thiết bị</h1>";
    page += "<p>Thiết bị: " + htmlEscape(deviceId) + "</p>";
    page += "<p>Lưu public key này lên server trước khi sử dụng.</p><textarea readonly rows='5' cols='48'>";
    page += htmlEscape(keys.publicPem()) + "</textarea><p><a href='/public-key'>Tải public key</a></p>";
    page += "<form method='post' action='/save'><p><label>Wi-Fi <input name='ssid' maxlength='32' required></label></p>"
      "<p><label>Mật khẩu Wi-Fi <input name='password' type='password' maxlength='63'></label></p>"
      "<p><label>URL web trò chơi <input name='url' type='url' maxlength='160' placeholder='Để trống để dùng URL mặc định của bản firmware'></label></p>"
      "<p><label>Chu kỳ đổi mã (giây) <input name='seconds' type='number' min='10' max='60' value='30' required></label></p>"
      "<button>Lưu và khởi động lại</button></form></body></html>";
    portal.sendHeader("Cache-Control", "no-store");
    portal.send(200, "text/html; charset=utf-8", page);
  });
  portal.on("/public-key", HTTP_GET, showPublicKey);
  portal.on("/save", HTTP_POST, saveSetup);
  // Android/iOS connectivity probes and unknown HTTP paths lead to setup.
  // Whether a login sheet opens automatically is controlled by the phone OS.
  portal.onNotFound([]() {
    portal.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    portal.send(302, "text/plain", "Mo trang thiet lap Wi-Fi.");
  });
  portal.begin();
  provisioning = true;
  String settingsUrl = "http://" + WiFi.softAPIP().toString() + "/";
  if (!Screen::wifiSetup(apName, String(SETUP_WIFI_PASSWORD), settingsUrl)) return false;
  Serial.printf("Setup Wi-Fi: %s\nSettings URL QR ready\nURL: %s\n", apName.c_str(), settingsUrl.c_str());
  Serial.printf("Setup key fingerprint: %s\n", keys.keyId().c_str());
  return true;
}

void connectWifi() {
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  lastWifiAttempt = millis();
  Screen::status("DANG NOI WIFI", wifiSsid);
}

bool loadConfig() {
  Preferences store;
  if (!store.begin("qr-config", false)) return false;
  bool configured = store.getBool("configured", false);
  wifiSsid = store.getString("ssid", "");
  wifiPassword = store.getString("pass", "");
  checkinUrl = store.getString("url", "");
  // Apply the user's event URL to devices already configured with our LAN test.
  // Only migrate that known old URL; keep saved Wi-Fi credentials and key intact.
  const String legacyTestUrl = "http://192.168.1.221:8765/checkin";
  if (configured && checkinUrl == legacyTestUrl && checkinUrl != DEFAULT_CHECKIN_URL) {
    checkinUrl = DEFAULT_CHECKIN_URL;
    if (store.putString("url", checkinUrl) != checkinUrl.length())
      Serial.println("Could not persist new QR URL; using it for this boot.");
  }
  if (checkinUrl.isEmpty()) checkinUrl = DEFAULT_CHECKIN_URL;
  qrSeconds = store.getUInt("interval", DEFAULT_QR_SECONDS);
  store.end();
  return configured && !wifiSsid.isEmpty() && qrSeconds >= 10 && qrSeconds <= 60 && validUrl(checkinUrl);
}

// Provisional server contract: docs/QR_PROTOCOL.md. This is NOT JWT or TOTP.
bool nextQr(time_t issuedAt, String& url) {
  String timestamp = String(static_cast<unsigned long>(issuedAt));
  String lifetime = String(qrSeconds);
  String payload = "v1\n" + deviceId + "\n" + keys.keyId() + "\n" + timestamp + "\n" + lifetime;
  String signature;
  if (!keys.sign(payload, signature)) return false;
  int fragmentAt = checkinUrl.indexOf('#');
  String address = fragmentAt >= 0 ? checkinUrl.substring(0, fragmentAt) : checkinUrl;
  String fragment = fragmentAt >= 0 ? checkinUrl.substring(fragmentAt) : "";
  url = address + "?v=1&dev=" + deviceId + "&kid=" + keys.keyId() +
      "&iat=" + timestamp + "&ttl=" + lifetime + "&sig=" + signature + fragment;
  return true;
}
}

void setup() {
  Serial.begin(115200); // Native USB CDC; do not initialize UART0 on screen pins 43/44.
  Serial.println("Starting display: SCLK42 MOSI41 DC43 RST44 CS=GND SPI_MODE3");
  Screen::begin();
  Serial.println("Display initialization returned; visual confirmation required.");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  Serial.printf("QR firmware 0.1.9 | device %s\n", deviceId.c_str());
  if (!loadConfig()) {
    initialized = startPortal();
  } else {
    initialized = keys.begin();
    if (initialized) {
      // The setup HTTP server and AP stay off in normal operation.
      configTime(0, 0, NTP_PRIMARY, NTP_SECONDARY);
      connectWifi();
      if (!CloudLink::begin()) Serial.println("DO link initialization failed; QR remains available.");
    }
  }
  if (!initialized) Screen::status("LOI KHOI TAO", "Khong phat QR. Kiem tra log USB.");
}

void loop() {
  handleUsbSetup();
  if (!initialized) { delay(100); return; }
  if (provisioning) {
    portalDns.processNextRequest();
    portal.handleClient();
    if (restartPending && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
    delay(5);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (timeReady) {
      // Fail closed when offline until the team's offline policy is decided.
      timeReady = false;
      displayedSlot = 0;
      Screen::status("MAT KET NOI", "Dang thu lai Wi-Fi. Tam dung QR.");
    }
    if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) connectWifi();
    delay(20);
    return;
  }
  time_t now = time(nullptr);
  if (now < 1704067200) { // Unset time must never become a valid-looking QR.
    static bool waitingDisplayed = false;
    if (!waitingDisplayed) {
      Screen::status("DONG BO GIO", "Dang cho server thoi gian...");
      waitingDisplayed = true;
    }
    delay(50);
    return;
  }
  if (!timeReady) {
    timeReady = true;
    Serial.println("Time available. Public key fingerprint:");
    Serial.println(keys.keyId());
    if (checkinUrl.isEmpty()) Screen::status("CHUA CO URL", "Chua phat QR check-in.\nCan cau hinh URL cua server.");
  }
  if (checkinUrl.isEmpty()) { delay(100); return; }
  time_t slot = now / qrSeconds;
  if (slot != displayedSlot || displayedKid != keys.keyId()) {
    String url;
    if (!nextQr(now, url) || !Screen::qr(url)) {
      Screen::status("LOI TAO QR", "Khong phat ma. Kiem tra URL / key.");
      delay(1000);
      return;
    }
    displayedSlot = slot;
    displayedKid = keys.keyId();
    Serial.printf("QR refreshed at %lu, valid for %lu seconds\n",
        static_cast<unsigned long>(now), static_cast<unsigned long>(qrSeconds));
    Serial.printf("QR target: %s\n", checkinUrl.c_str());
  }
  CloudLink::tick(keys, deviceId);
  delay(20);
}

