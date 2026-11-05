#pragma once
#include <Arduino.h>
#include "WifiProfiles.h"

class SavedNetworks {
public:
  bool load(const String& legacySsid, const String& legacyPassword);
  bool remember(const String& ssid, const String& password);
  bool forget(const String& ssid);
  bool clear();
  size_t size() const { return profiles_.count; }
  const WifiProfile& at(size_t index) const { return profiles_.entries[index]; }
  int find(const String& ssid) const { return profiles_.find(ssid.c_str()); }
private:
  bool persist(const WifiProfiles& profiles);
  WifiProfiles profiles_;
};
