#pragma once
#include <Arduino.h>

namespace Screen {
void begin();
void status(const String& title, const String& detail = "");
bool qr(const String& text);
bool wifiSetup(const String& ssid, const String& password, const String& settingsUrl);
}

