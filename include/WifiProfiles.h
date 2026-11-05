#pragma once
#include <stddef.h>
#include <string.h>

struct WifiProfile {
  char ssid[33] = {};
  char password[64] = {};
};

// Most recently saved first; updating a network never duplicates its SSID.
struct WifiProfiles {
  static constexpr size_t capacity = 5;
  WifiProfile entries[capacity];
  size_t count = 0;

  int find(const char* ssid) const {
    for (size_t i = 0; i < count; ++i) if (strcmp(entries[i].ssid, ssid) == 0) return static_cast<int>(i);
    return -1;
  }

  bool remember(const char* ssid, const char* password) {
    size_t sn = strlen(ssid), pn = strlen(password);
    if (sn == 0 || sn > 32 || pn > 63 || (pn != 0 && pn < 8)) return false;
    WifiProfile entry;
    memcpy(entry.ssid, ssid, sn + 1);
    memcpy(entry.password, password, pn + 1);
    int found = find(ssid);
    size_t move = found >= 0 ? static_cast<size_t>(found) : (count < capacity ? count++ : capacity - 1);
    while (move > 0) { entries[move] = entries[move - 1]; --move; }
    entries[0] = entry;
    return true;
  }
};
