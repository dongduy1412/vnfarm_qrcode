#pragma once
#include <Arduino.h>
namespace WebSocketLink {
bool begin(const String& origin,const String& deviceId,const String& token,const String& keyId);
void tick();
void diagnostics();
bool isConnected(); // True only after the server ACKs this boot report.
}
