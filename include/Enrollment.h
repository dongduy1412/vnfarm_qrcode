#pragma once
#include <Arduino.h>
#include "DeviceKeys.h"
class Enrollment {
public:
  bool begin(const String& origin, DeviceKeys& keys, const String& serial);
  void tick(DeviceKeys& keys);
  // Explicit operator confirmation: both origins refer to the same backend.
  static bool copyIdentity(const String& from, const String& to, DeviceKeys& keys);
  bool ready() const { return phase_=="ready"; }
  const String& deviceId() const { return did_; }
  const String& token() const { return token_; }
  const String& keyId() const { return kid_; }
  const String& status() const { return status_; }
private:
  bool save();
  String origin_,slot_,serial_,phase_,registration_,nonce_,did_,token_,kid_,status_;
  int64_t expires_=0;
  uint32_t retryAt_=0;
};
