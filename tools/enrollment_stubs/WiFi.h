#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED=3;
struct WifiMock{int status()const{return WL_CONNECTED;}};
inline WifiMock WiFi;
class WiFiClient{};
