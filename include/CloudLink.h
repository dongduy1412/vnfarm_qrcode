#pragma once
#include <Arduino.h>
#include "DeviceKeys.h"

namespace CloudLink {
bool begin();
void tick(DeviceKeys& keys, const String& deviceId);
}
