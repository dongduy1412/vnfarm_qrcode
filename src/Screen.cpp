#include "Screen.h"
#include "AppConfig.h"
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <qrcode.h>
#include "BrandedQr.h"

namespace {
Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, TFT_RST);
}

void Screen::begin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  // CS is permanently low in the supplied wiring. Try mode 3 for this panel;
  // successful initialization still requires visual confirmation on hardware.
  display.init(TFT_WIDTH, TFT_HEIGHT, SPI_MODE3);
  display.setRotation(TFT_ROTATION);
  display.invertDisplay(TFT_INVERT);
  display.setTextWrap(true);
  status("VN FARM", "Khoi dong...");
}

void Screen::status(const String& title, const String& detail) {
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.setCursor(8, 12);
  display.println(title);
  display.setTextSize(1);
  display.setCursor(8, 48);
  display.println(detail);
}

namespace {
bool drawQr(const String& text, int areaY, int areaHeight) {
  QRCode code;
  static uint8_t buffer[4096];
  if (qrcode_getBufferSize(QR_MAX_VERSION) > sizeof(buffer)) return false;
  // ricmoo 0.0.1 does NOT reject over-capacity input reliably. Select using
  // byte-mode L capacities BEFORE calling it; URL payloads are ASCII byte-mode.
  static constexpr uint16_t capacities[] = {17, 32, 53, 78, 106, 134, 154, 192, 230, 271, 321, 367, 425};
  uint8_t version = 1;
  while (version <= QR_MAX_VERSION && text.length() > capacities[version - 1]) ++version;
  if (version > QR_MAX_VERSION) return false;
  if (qrcode_initText(&code, buffer, version, ECC_LOW, text.c_str()) != 0) return false;
  constexpr int quietZone = 4;
  int scale = min(TFT_WIDTH, areaHeight) / (code.size + quietZone * 2);
  if (scale < 1) return false;
  int left = (TFT_WIDTH - code.size * scale) / 2;
  int top = areaY + (areaHeight - code.size * scale) / 2;
  display.fillScreen(ST77XX_WHITE);
  display.startWrite();
  for (int y = 0; y < code.size; ++y) {
    for (int x = 0; x < code.size; ++x) {
      if (qrcode_getModule(&code, x, y))
        display.writeFillRect(left + x * scale, top + y * scale, scale, scale, ST77XX_BLACK);
    }
  }
  display.endWrite();
  return true;
}

void centeredLine(const String& text, int y) {
  display.setTextSize(1);
  display.setTextColor(ST77XX_BLACK);
  display.setCursor(max(0, (TFT_WIDTH - static_cast<int>(text.length()) * 6) / 2), y);
  display.print(text);
}
}

bool Screen::qr(const String& text) {
  display.startWrite();
  bool ok = BrandedQr::render(display, text.c_str());
  display.endWrite();
  return ok;
}

bool Screen::wifiSetup(const String& ssid, const String& password, const String& settingsUrl) {
  // Join the visible AP in phone settings first. This QR opens its web console.
  // Credentials stay outside the QR and remain visible for manual Wi-Fi login.
  if (!drawQr(settingsUrl, 48, 156)) return false;
  centeredLine("1. KET NOI WI-FI", 5);
  centeredLine(ssid, 18);
  centeredLine("Mat khau: " + password, 31);
  centeredLine("2. QUET QR DE MO CAU HINH", 218);
  centeredLine("http://192.168.4.1", 232);
  return true;
}

