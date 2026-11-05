#pragma once
#include <Arduino.h>
#include "DeviceKeys.h"

namespace CloudLink {
struct Event {
  enum Type { Command,Acknowledged } type;
  char cmdId[65]={};
  char command[24]={};
};
bool begin();
void tick(DeviceKeys& keys, const String& deviceId,const char* pendingAck="");
bool acknowledge(DeviceKeys& keys,const String& deviceId,const char* cmdId);
bool nextEvent(Event& event);
}
