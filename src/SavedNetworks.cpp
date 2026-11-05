#include "SavedNetworks.h"
#include <ArduinoJson.h>
#include <Preferences.h>

bool SavedNetworks::persist(const WifiProfiles& profiles) {
  DynamicJsonDocument json(2048);
  json["version"] = 1;
  JsonArray networks = json.createNestedArray("networks");
  for (size_t i = 0; i < profiles.count; ++i) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = profiles.entries[i].ssid;
    network["password"] = profiles.entries[i].password;
  }
  if (json.overflowed()) return false;
  String encoded;
  serializeJson(json, encoded);
  Preferences store;
  if (!store.begin("qr-networks", false)) return false;
  // One NVS string is an atomic snapshot; never clear the old list first.
  bool ok = store.putString("list", encoded) == encoded.length() && store.getString("list", "") == encoded;
  store.end();
  return ok;
}

bool SavedNetworks::load(const String& legacySsid, const String& legacyPassword) {
  Preferences store;
  if (!store.begin("qr-networks", false)) return false;
  bool exists = store.isKey("list");
  String encoded = store.getString("list", "");
  store.end();
  if (!exists) {
    // Only the last network exists in old firmware; earlier overwritten ones
    // cannot be recovered. Preserve that one even when USB setup was requested.
    if (legacySsid.isEmpty()) return true;
    return remember(legacySsid, legacyPassword);
  }
  if (encoded.length() > 4096) return false;
  DynamicJsonDocument json(4096);
  if (deserializeJson(json, encoded) || json["version"] != 1 || !json["networks"].is<JsonArray>()) return false;
  JsonArray networks = json["networks"];
  if (networks.size() > WifiProfiles::capacity) return false;
  WifiProfiles loaded;
  for (size_t i = networks.size(); i > 0; --i) {
    JsonObject network = networks[i - 1];
    if (!network["ssid"].is<const char*>() || !network["password"].is<const char*>() ||
        loaded.find(network["ssid"]) >= 0 || !loaded.remember(network["ssid"], network["password"])) return false;
  }
  profiles_ = loaded;
  return true;
}

bool SavedNetworks::remember(const String& ssid, const String& password) {
  WifiProfiles next = profiles_;
  if (!next.remember(ssid.c_str(), password.c_str()) || !persist(next)) return false;
  profiles_ = next;
  return true;
}

bool SavedNetworks::forget(const String& ssid) {
  WifiProfiles next=profiles_;
  next.forget(ssid.c_str());
  if(!persist(next))return false;
  profiles_=next;
  return true;
}

bool SavedNetworks::clear() {
  WifiProfiles empty;
  if(!persist(empty))return false;
  profiles_=empty;
  return true;
}
