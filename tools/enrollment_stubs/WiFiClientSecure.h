#pragma once
#include "WiFi.h"
class WiFiClientSecure:public WiFiClient{public:void setCACert(const char*){}void setHandshakeTimeout(int){}int lastError(char*,size_t){return 0;}};
