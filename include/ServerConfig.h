#pragma once

// Build-time configuration only. No private keys or admin tokens belong here.
// Check-in (phone -> Worker) and management (board -> Worker) may use different hosts.
#ifndef QR_DEFAULT_CHECKIN_URL
#define QR_DEFAULT_CHECKIN_URL ""
#endif
#ifndef QR_CLOUD_BASE_URL
#define QR_CLOUD_BASE_URL ""
#endif
// Personal-test extension, NOT part of the mentor's poll/ACK contract.
#ifndef QR_CLOUD_HEARTBEAT_ENABLED
#define QR_CLOUD_HEARTBEAT_ENABLED 0
#endif
#ifndef QR_DEVICE_POLL_INTERVAL_MS
#define QR_DEVICE_POLL_INTERVAL_MS 60000UL
#endif

namespace ServerConfig {
// C++11 constexpr recursion keeps validation compatible with Arduino's toolchain.
constexpr bool startsWith(const char* text, const char* prefix) {
  return !*prefix || (*text && *text == *prefix && startsWith(text + 1, prefix + 1));
}
constexpr bool alphaNumeric(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
constexpr bool validHost(const char* text, bool label = false, char previous = 0) {
  return !*text ? label && previous != '-' :
      *text == '.' ? label && previous != '-' && validHost(text + 1, false, '.') :
      (alphaNumeric(*text) || (*text == '-' && label)) && validHost(text + 1, true, *text);
}
constexpr bool validManagementOrigin(const char* url) {
  return startsWith(url, "https://") && validHost(url + 8);
}
constexpr const char* managementOrigin = QR_CLOUD_BASE_URL;
constexpr bool heartbeatEnabled = QR_CLOUD_HEARTBEAT_ENABLED;
constexpr unsigned long pollIntervalMs = QR_DEVICE_POLL_INTERVAL_MS;
static_assert(!*managementOrigin || validManagementOrigin(managementOrigin),
              "QR_CLOUD_BASE_URL must be an HTTPS hostname origin without trailing slash");
static_assert(QR_CLOUD_HEARTBEAT_ENABLED == 0 || QR_CLOUD_HEARTBEAT_ENABLED == 1,
              "QR_CLOUD_HEARTBEAT_ENABLED must be 0 or 1");
static_assert(pollIntervalMs >= 1000UL && pollIntervalMs <= 86400000UL,
              "Poll interval must be between 1 second and 24 hours");
}
