#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <time.h>
#include <esp_sntp.h>
#include "NtpGate.h"
#include "AppConfig.h"
#include "ServerOrigin.h"
#include "WebSocketLink.h"
#include "Enrollment.h"
#include "DeviceKeys.h"
#include "Screen.h"
#include "CloudLink.h"
#include "SavedNetworks.h"
#include "QrContract.h"
#include "SetupPageAsset.h"
#include "WifiCommands.h"

namespace {
DeviceKeys keys;
SavedNetworks networks;
WifiCommands wifiCommands;
Enrollment enrollment;
bool projectMode=false,identityInitialized=false,websocketStarted=false;
String enrollmentScreen;
WebServer portal(80);
DNSServer portalDns;
String wifiSsid, wifiPassword, checkinUrl, deviceId;
String workerOrigin = ServerConfig::managementOrigin;
bool workerHeartbeat = ServerConfig::heartbeatEnabled;
uint32_t qrSeconds = DEFAULT_QR_SECONDS;
bool provisioning = false;
bool initialized = false;
bool restartPending = false;
bool timeReady = false;
NtpGate ntpGate;
uint32_t loggedNtpSync = 0;
uint32_t ntpWaitLogAt = 0;
void onNtpSync(struct timeval* value) {
  // Runs on the SNTP task. No display, NVS or Serial work here.
  if (value) ntpGate.synchronized(value->tv_sec);
}
uint32_t restartAt = 0;
uint32_t lastWifiAttempt = 0;
size_t wifiIndex = 0;
bool wifiWasConnected = false;
bool waitingDisplayed = false;
bool wifiFallbackToSetup=false;
uint32_t wifiSearchStarted=0;
time_t displayedSlot = 0;
String displayedKid;
String displayedUrl;
String usbCommand;
bool remoteRestartPending=false,remoteAckReceived=false;
uint32_t remoteRestartAt=0;
String remoteCommandId;

void handleRemoteCommands() {
  CloudLink::Event event;
  while(CloudLink::nextEvent(event)) {
    if(event.type==CloudLink::Event::Acknowledged) {
      if(remoteRestartPending&&remoteCommandId==event.cmdId)remoteAckReceived=true;
      else if(wifiCommands.acknowledge(event.cmdId))Serial.printf("Device command ACK confirmed: %s\n",event.cmdId);
      continue;
    }
    if(remoteRestartPending)continue;
    auto result=wifiCommands.accept(event.cmdId,event.command,WiFi.SSID(),networks);
    if(result==WifiCommands::Failed) {Serial.println("Device command storage failed; not acknowledged");continue;}
    if(result==WifiCommands::Duplicate) {CloudLink::acknowledge(keys,deviceId,event.cmdId);continue;}
    remoteCommandId=event.cmdId;remoteRestartPending=true;remoteAckReceived=false;remoteRestartAt=millis()+12000;
    displayedUrl="";
    Screen::status("DAT LAI WI-FI","Dang gui xac nhan cho server...");
    Serial.printf("Device command applied: %s | saved profiles=%u | signing key preserved\n",event.command,static_cast<unsigned>(networks.size()));
    CloudLink::acknowledge(keys,deviceId,event.cmdId);
  }
  if(remoteRestartPending&&(remoteAckReceived||static_cast<int32_t>(millis()-remoteRestartAt)>=0)) {
    // Managed profiles are already updated. Clear the driver's native STA
    // credentials only after attempting ACK over the still-live RAM connection.
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    WiFi.disconnect(false,true);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(remoteAckReceived) {
      wifiCommands.acknowledge(remoteCommandId.c_str());
      Serial.println("Remote Wi-Fi command acknowledged; restarting");
    } else Serial.println("ACK still pending; will retry after Wi-Fi is available");
    Serial.flush();delay(100);ESP.restart();
  }
}

// Local USB maintenance only. Keep the signing key and saved settings;
// mark setup incomplete so the portal also survives a power cycle.
void handleUsbSetup() {
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (usbCommand.length() < 192) usbCommand += ch;
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
    if (usbCommand == "WIPE_WIFI") {
      // Explicit local maintenance: works offline, preserves key/server settings.
      char id[48];
      snprintf(id, sizeof(id), "c_usb_%08lx_%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(millis()));
      auto result = wifiCommands.accept(id, "wipe_wifi", WiFi.SSID(), networks);
      if (result == WifiCommands::Applied && wifiCommands.acknowledge(id)) {
        Serial.println("USB Wi-Fi wipe complete; saved profiles=0; key/server preserved. Restarting.");
        Serial.flush();
        esp_wifi_set_storage(WIFI_STORAGE_FLASH);
        WiFi.disconnect(false, true);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        delay(100);
        ESP.restart();
      } else Serial.println("USB Wi-Fi wipe failed; inspect storage before retrying.");
    }
    if (usbCommand.startsWith("SERVER_URL ")) {
      // USB lab convenience; the normal operator flow uses the setup form.
      String origin=usbCommand.substring(11);origin.trim();
      while(origin.endsWith("/"))origin.remove(origin.length()-1);
      if(ServerOrigin::valid(origin.c_str())&&origin.length()<=160){
        Preferences store;bool ok=store.begin("qr-config",false);
        if(ok){
          bool configured=store.getBool("configured",false);String url=origin+"/checkin";
          ok=store.putBool("configured",false)==1&&store.putString("worker",origin)==origin.length()&&
              store.putString("url",url)==url.length()&&store.putBool("heartbeat",false)==1&&
              store.putUInt("server_v",1)==sizeof(uint32_t);
          if(ok)ok=store.putBool("configured",configured)==1;
          store.end();
        }
        if(ok){Serial.println("Server URL saved; Wi-Fi/key preserved. Restarting.");Serial.flush();delay(100);ESP.restart();}
        else Serial.println("Server URL save failed; setup required.");
      }else Serial.println("Invalid server origin; not saved.");
    }
    if (usbCommand == "DEVICE_INFO") {
      Serial.printf("Device info: firmware=0.4.1 did=%s server=%s setup=%d wifi=%d ntp=%lu identity=%d ws=%d status=%s\n",
          deviceId.c_str(),workerOrigin.c_str(),provisioning,static_cast<int>(WiFi.status()),
          static_cast<unsigned long>(ntpGate.lastSync()),enrollment.ready(),WebSocketLink::isConnected(),enrollment.status().c_str());
    }
    if (usbCommand == "QR_INFO") {
      if (initialized && !provisioning && !remoteRestartPending && timeReady && ntpGate.ready(time(nullptr)) && WiFi.status() == WL_CONNECTED && displayedUrl.length()) {
        // Explicit USB diagnostic for interoperability tests; routine logs omit sig.
        Serial.printf("QR public key: %s\nQR test URL: %s\n", keys.publicRaw().c_str(), displayedUrl.c_str());
      } else {
        const char* reason = !initialized ? "initialization" : provisioning ? "setup" :
            remoteRestartPending ? "remote_restart" : WiFi.status() != WL_CONNECTED ? "wifi" :
            !ntpGate.ready(time(nullptr)) ? "ntp" : !timeReady ? "loop_pending" : "qr_render";
        Serial.printf("QR not ready; reason=%s init=%d portal=%d time=%d wifi=%d url=%u ntp=%lu uptime_ms=%lu reset=%d\n",
            reason, initialized, provisioning, timeReady, static_cast<int>(WiFi.status()),
            static_cast<unsigned>(displayedUrl.length()), static_cast<unsigned long>(ntpGate.lastSync()),
            static_cast<unsigned long>(millis()), static_cast<int>(esp_reset_reason()));
      }
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
  // The operator selects the origin; route remains /checkin.
  if (url.length() == 0) return false;
  if (url.indexOf('#') >= 0 || !url.endsWith("/checkin")) return false;
  if (url.length() > MAX_CHECKIN_URL_LENGTH) return false;
  if (!ServerOrigin::valid(url.substring(0, url.length() - 8).c_str())) return false;
  for (size_t i = 0; i < url.length(); ++i)
    if (url[i] <= 32 || url[i] >= 127) return false;
  return true;
}

void showPublicKey() {
  portal.sendHeader("Cache-Control", "no-store");
  portal.sendHeader("Content-Disposition", "attachment; filename=public-key.txt");
  portal.send(200, "text/plain; charset=utf-8", keys.publicRaw());
}

void saveSetup() {
  Serial.printf("SETUP save received: uptime_ms=%lu mode=%s\n", static_cast<unsigned long>(millis()), portal.hasArg("saved") ? "saved-network" : "new-network");
  String ssid = portal.arg("ssid");
  String password = portal.arg("password");
  String origin = portal.hasArg("worker") ? portal.arg("worker") : workerOrigin;
  origin.trim();
  while (origin.endsWith("/")) origin.remove(origin.length() - 1);
  String url = portal.hasArg("checkin") ? portal.arg("checkin") : checkinUrl;
  url.trim();
  if (url.isEmpty()) url = origin + "/checkin";
  bool heartbeat = portal.hasArg("worker") ? portal.arg("heartbeat") == "1" : workerHeartbeat;
  if (origin.length() > 160 || !ServerOrigin::valid(origin.c_str()) || !validUrl(url)) {
    Serial.println("SETUP rejected: invalid server URL");
    portal.send(400, "text/plain; charset=utf-8", "Link goc phai la HTTPS hoac HTTP IP LAN, co the kem port 1-65535. Khong them path/query. Link QR ket thuc /checkin.");
    return;
  }
  String secondsText = portal.arg("seconds");
  if (portal.hasArg("saved")) {
    int selected = networks.find(portal.arg("saved"));
    if (selected < 0) { portal.send(400, "text/plain", "Mang chua duoc luu."); return; }
    ssid = networks.at(selected).ssid;
    password = networks.at(selected).password;
    if (!portal.hasArg("seconds")) secondsText = String(qrSeconds);
  }
  url.trim();
  bool digitsOnly = secondsText.length() > 0 && secondsText.length() <= 2;
  for (size_t i = 0; i < secondsText.length(); ++i)
    if (secondsText[i] < '0' || secondsText[i] > '9') digitsOnly = false;
  long seconds = secondsText.toInt();
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
      (!password.isEmpty() && password.length() < 8) || !validUrl(url) ||
      !digitsOnly || seconds < 10 || seconds > 60) {
    Serial.println("SETUP rejected: invalid Wi-Fi/interval fields");
    portal.send(400, "text/plain; charset=utf-8", "Thong tin khong hop le. Chu ky 10-60 giay; kiem tra URL va Wi-Fi.");
    return;
  }
  // Log state only, never Wi-Fi passwords or request bodies.
  Serial.println("SETUP Wi-Fi connection test starting (15s deadline)");
  // Do not commit credentials before testing them, so a typo cannot lock setup.
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    int status = static_cast<int>(WiFi.status());
    Serial.printf("SETUP Wi-Fi connection failed: status=%d elapsed_ms=%lu\n", status, static_cast<unsigned long>(millis()-start));
    WiFi.disconnect();
    portal.send(400, "text/plain; charset=utf-8", String("WIFI_CONNECT_FAILED: status=") + status + ". Khong ket noi duoc Wi-Fi trong 15s. Kiem tra mang 2.4 GHz, ten/mat khau.");
    return;
  }
  Serial.println("SETUP Wi-Fi connected; saving profiles/configuration");
  if (!networks.remember(ssid, password)) {
    portal.send(500, "text/plain", "Khong luu duoc danh sach Wi-Fi. Hay thu lai.");
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
      store.putString("worker", origin) == origin.length() &&
      store.putBool("heartbeat", heartbeat) == 1 &&
      store.putUInt("server_v", 1) == sizeof(uint32_t) &&
      store.putUInt("interval", seconds) == sizeof(uint32_t) && store.putBool("fallback",false)==1;
  ok = ok && store.getString("ssid", "__missing") == ssid &&
      store.getString("pass", "__missing") == password &&
      store.getString("url", "__missing") == url &&
      store.getString("worker", "__missing") == origin &&
      store.getBool("heartbeat", !heartbeat) == heartbeat &&
      store.getUInt("server_v", 0) == 1 && store.getUInt("interval", 0) == seconds;
  if (ok) ok = store.putBool("configured", true) == 1;
  store.end();
  if (!ok) {
    Serial.println("SETUP config write/readback failed");
    portal.send(500, "text/plain", "Luu cau hinh that bai. Chua danh dau thiet lap thanh cong.");
    return;
  }
  Serial.println("SETUP configuration saved; restart in 2 seconds (Worker API not yet verified)");
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
    String page(FPSTR(SETUP_PAGE));
    page.replace("{{DEVICE_ID}}", htmlEscape(deviceId));
    page.replace("{{PUBLIC_KEY}}", htmlEscape(keys.publicRaw()));
    page.replace("{{INTERVAL}}", String(qrSeconds));
    page.replace("{{CHECKIN_URL}}", htmlEscape(checkinUrl));
    page.replace("{{WORKER_ORIGIN}}", htmlEscape(workerOrigin));
    page.replace("{{HEARTBEAT_CHECKED}}", workerHeartbeat ? "checked" : "");
    page.replace("{{SAVED_HIDDEN}}", networks.size() ? "" : "hidden");
    String options;
    for (size_t i = 0; i < networks.size(); ++i) {
      String name = htmlEscape(String(networks.at(i).ssid));
      options += "<option value='" + name + "'>" + name + "</option>";
    }
    // User-controlled SSIDs are escaped and inserted last, so template-like
    // text inside a saved SSID is never evaluated as another placeholder.
    page.replace("{{SAVED_OPTIONS}}", options);
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

void connectWifi(bool next = false) {
  if (!networks.size()) return;
  if (next) wifiIndex = (wifiIndex + 1) % networks.size();
  wifiSsid = networks.at(wifiIndex).ssid;
  wifiPassword = networks.at(wifiIndex).password;
  WiFi.disconnect(false, false);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  lastWifiAttempt = millis();
  Screen::status("DANG NOI WIFI", wifiSsid);
  Serial.printf("Trying saved Wi-Fi %u/%u\n", static_cast<unsigned>(wifiIndex + 1), static_cast<unsigned>(networks.size()));
}

bool loadConfig() {
  Preferences store;
  if (!store.begin("qr-config", false)) return false;
  bool configured = store.getBool("configured", false);
  wifiSsid = store.getString("ssid", "");
  wifiPassword = store.getString("pass", "");
  checkinUrl = store.getString("url", "");
  // Old builds forced the compile-time URL. Migrate once, never overwrite
  // a server selected by the operator on subsequent boots or app uploads.
  bool customServer = store.getUInt("server_v", 0) == 1;
  workerOrigin = customServer ? store.getString("worker", "") : ServerConfig::managementOrigin;
  workerHeartbeat = customServer ? store.getBool("heartbeat", false) : ServerConfig::heartbeatEnabled;
  if (!customServer) checkinUrl = DEFAULT_CHECKIN_URL;
  qrSeconds = store.getUInt("interval", DEFAULT_QR_SECONDS);
  wifiFallbackToSetup=store.getBool("fallback",false);
  store.end();
  return configured && !wifiSsid.isEmpty() && qrSeconds >= 10 && qrSeconds <= 60 && validUrl(checkinUrl) &&
      workerOrigin.length() <= 160 && ServerOrigin::valid(workerOrigin.c_str());
}

// Provisional server contract: docs/QR_PROTOCOL.md. This is NOT JWT or TOTP.
bool nextQr(time_t issuedAt, String& url) {
  uint32_t rounded = QrContract::timestamp(static_cast<uint32_t>(issuedAt), qrSeconds);
  char payload[64];
  if (!QrContract::message(payload, sizeof(payload), deviceId.c_str(), rounded)) return false;
  String signature;
  if (!keys.signQr(String(payload), signature)) return false;
  url = checkinUrl + "?v=1&did=" + deviceId + "&t=" + String(rounded) + "&sig=" + signature;
  return true;
}
}

void setup() {
  Serial.begin(115200); // Native USB CDC; do not initialize UART0 on screen pins 43/44.
  Serial.println("Starting display: SCLK42 MOSI41 DC43 RST44 CS=GND SPI_MODE3");
  Screen::begin();
  Serial.println("Display initialization returned; visual confirmation required.");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false); // Our retry loop rotates through saved profiles.
  WiFi.mode(WIFI_STA);
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  Serial.printf("QR firmware 0.4.1 | device %s\n", deviceId.c_str());
  bool configured = loadConfig();
  bool networksReady = networks.load(wifiSsid, wifiPassword);
  if(!networksReady||!wifiCommands.begin(networks)) {
    Screen::status("LOI CAU HINH","Kiem tra bo nho lenh / Wi-Fi.");
    Serial.println("Could not recover Wi-Fi command state; refusing partial recovery.");
    return;
  }
  // A recovered operation may have changed the profiles and configured flag.
  configured=loadConfig();networksReady=networks.load(wifiSsid,wifiPassword);
  Serial.printf("Saved Wi-Fi profiles: %u\n", static_cast<unsigned>(networks.size()));
  if (!networksReady) Serial.println("Could not read/migrate Wi-Fi profiles; setup required.");
  if (!configured || !networksReady || !networks.size()) {
    initialized = startPortal();
  } else {
    initialized = keys.begin();
    if (initialized) {
      // The setup HTTP server and AP stay off in normal operation.
      sntp_set_time_sync_notification_cb(onNtpSync);
      sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
      configTime(0, 0, NTP_PRIMARY, NTP_SECONDARY);
      Serial.println("NTP: waiting for successful sync in this boot; periodic interval=3600s");
      connectWifi();
      wifiSearchStarted=millis();
      projectMode = workerOrigin != "https://vnfarm-qrcode-device-link.dong141220047.workers.dev";
      if(projectMode){
        identityInitialized=enrollment.begin(workerOrigin,keys,deviceId);
        if(!identityInitialized){Serial.println("Identity storage invalid; enrollment blocked");Screen::status("LOI IDENTITY","Kiem tra cau hinh server / bo nho.");}
      }else if(!CloudLink::begin(workerOrigin,workerHeartbeat))Serial.println("Lab cloud link initialization failed");
    }
  }
  if (!initialized) Screen::status("LOI KHOI TAO", "Khong phat QR. Kiem tra log USB.");
}

void loop() {
  handleUsbSetup();
  if (!initialized) { delay(100); return; }
  handleRemoteCommands();
  if(remoteRestartPending){delay(20);return;}
  if (provisioning) {
    portalDns.processNextRequest();
    portal.handleClient();
    if (restartPending && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
    delay(5);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if(wifiFallbackToSetup&&static_cast<uint32_t>(millis()-wifiSearchStarted)>WIFI_RETRY_MS*networks.size()+5000) {
      Preferences config;
      if(config.begin("qr-config",false)) {
        bool saved=config.putBool("configured",false)==1;config.end();
        if(saved){Serial.println("No remaining saved Wi-Fi reachable; opening setup");Serial.flush();ESP.restart();}
      }
    }
    if (wifiWasConnected) {
      wifiWasConnected = false;
      waitingDisplayed = false;
      connectWifi(); // Retry the last successful profile before trying others.
    }
    if (timeReady) {
      // Fail closed when offline until the team's offline policy is decided.
      timeReady = false;
      displayedUrl = "";
      displayedSlot = 0;
      Serial.printf("QR paused: wifi=%d uptime_ms=%lu\n", static_cast<int>(WiFi.status()), static_cast<unsigned long>(millis()));
      Screen::status("MAT KET NOI", "Dang thu lai Wi-Fi. Tam dung QR.");
    }
    if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) connectWifi(true);
    delay(20);
    return;
  }
  if (!wifiWasConnected) {
    wifiWasConnected = true;
    Serial.printf("Saved Wi-Fi connected: profile %u/%u\n", static_cast<unsigned>(wifiIndex + 1), static_cast<unsigned>(networks.size()));
    if(wifiFallbackToSetup) {
      Preferences config;if(config.begin("qr-config",false)){config.putBool("fallback",false);config.end();}
      wifiFallbackToSetup=false;
    }
  }
  time_t now = time(nullptr);
  if (!ntpGate.ready(now)) {
    timeReady = false;
    displayedUrl = "";
    displayedSlot = 0;
    if (!waitingDisplayed) {
      Screen::status("DONG BO GIO", "Dang cho NTP...\nChua phat QR. Tu dong thu lai.");
      waitingDisplayed = true;
    }
    if (static_cast<uint32_t>(millis()-ntpWaitLogAt)>=60000) {
      Serial.println("NTP not synchronized; QR blocked, SNTP continues retrying");
      ntpWaitLogAt=millis();
    }
    delay(50);
    return;
  }
  waitingDisplayed = false;
  uint32_t confirmedSync = ntpGate.lastSync();
  if (confirmedSync != loggedNtpSync) {
    Serial.printf("NTP sync confirmed: epoch=%lu uptime_ms=%lu interval_ms=%lu\n",static_cast<unsigned long>(confirmedSync),static_cast<unsigned long>(millis()),static_cast<unsigned long>(sntp_get_sync_interval()));
    loggedNtpSync = confirmedSync;
  }
  if (!timeReady) {
    timeReady = true;
    Serial.println("Time available. Public key fingerprint:");
    Serial.println(keys.keyId());
    if (checkinUrl.isEmpty()) Screen::status("CHUA CO URL", "Chua phat QR check-in.\nCan cau hinh URL cua server.");
  }
  if(projectMode){
    if(!identityInitialized){delay(50);return;}
    enrollment.tick(keys);
    if(!enrollment.ready()){
      if(enrollmentScreen!=enrollment.status()){
        enrollmentScreen=enrollment.status();Screen::status(enrollmentScreen,"Tu dong xac thuc voi server.");
      }
      delay(20);return;
    }
    deviceId=enrollment.deviceId();qrSeconds=30;
    if(!websocketStarted){
      websocketStarted=WebSocketLink::begin(workerOrigin,deviceId,enrollment.token(),enrollment.keyId());
      if(!websocketStarted){Screen::status("LOI WEBSOCKET","Khong khoi tao duoc ket noi.");delay(1000);return;}
    }
    WebSocketLink::tick();
    if(!WebSocketLink::isConnected()){
      displayedUrl="";displayedSlot=0;
      if(enrollmentScreen!="DANG NOI SERVER"){enrollmentScreen="DANG NOI SERVER";Screen::status(enrollmentScreen,"Dang cho xac nhan BOOT...");}
      delay(20);return;
    }
    enrollmentScreen="";
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
    displayedUrl = url;
    displayedKid = keys.keyId();
    Serial.printf("QR refreshed: t=%lu, interval=%lu seconds (TTL is server policy)\n",
        static_cast<unsigned long>(QrContract::timestamp(static_cast<uint32_t>(now), qrSeconds)), static_cast<unsigned long>(qrSeconds));
    Serial.printf("QR target: %s\n", checkinUrl.c_str());
  }
  if(!projectMode)CloudLink::tick(keys, deviceId,wifiCommands.pendingAck());
  delay(20);
}

