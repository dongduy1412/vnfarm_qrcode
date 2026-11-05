#pragma once
#include <Arduino.h>

// Source: docs/s3tim154st7789.jpg provided by the user.
// TX/RX are GPIO43/44 in the ESP32-S3 Arduino variant, NOT Serial UART pins here.
constexpr int TFT_SCLK = 42;
constexpr int TFT_MOSI = 41;
constexpr int TFT_DC = 43;
constexpr int TFT_RST = 44;
constexpr int TFT_CS = -1;  // Wired to GND; display is the only device on this SPI bus.
constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 240;
constexpr int TFT_ROTATION = 0;  // Must be confirmed on the physical display.
constexpr bool TFT_INVERT = true;

// Fixed setup AP password explicitly selected by the user.
constexpr char SETUP_WIFI_PASSWORD[] = "12345678";
static_assert(sizeof(SETUP_WIFI_PASSWORD) >= 9 && sizeof(SETUP_WIFI_PASSWORD) <= 64,
              "Setup Wi-Fi password must contain 8-63 characters");

// Implementation defaults, NOT values explicitly decided by the lead.
constexpr uint32_t DEFAULT_QR_SECONDS = 30;
constexpr const char* NTP_PRIMARY = "time.google.com";
constexpr const char* NTP_SECONDARY = "pool.ntp.org";
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint8_t QR_MAX_VERSION = 13;
constexpr size_t MAX_CHECKIN_URL_LENGTH = 160;
#ifndef QR_ALLOW_HTTP_TEST
#define QR_ALLOW_HTTP_TEST 0
#endif
#ifndef QR_DEFAULT_CHECKIN_URL
#define QR_DEFAULT_CHECKIN_URL "https://vnfam.999.vn/#/"
#endif
constexpr const char* DEFAULT_CHECKIN_URL = QR_DEFAULT_CHECKIN_URL;

