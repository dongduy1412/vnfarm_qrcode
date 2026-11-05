#include "WifiProfiles.h"
#include <assert.h>
#include <stdio.h>

int main() {
  WifiProfiles profiles;
  assert(!profiles.remember("", "12345678"));
  assert(!profiles.remember("test", "short"));
  assert(profiles.remember("home", "12345678"));
  assert(profiles.remember("office", "abcdefgh"));
  assert(profiles.count == 2 && profiles.find("home") == 1);
  assert(profiles.remember("home", "new-password"));
  assert(profiles.count == 2 && profiles.find("home") == 0);
  assert(strcmp(profiles.entries[0].password, "new-password") == 0);
  assert(profiles.remember("open", ""));
  assert(profiles.remember("four", "12345678"));
  assert(profiles.remember("five", "12345678"));
  assert(profiles.remember("six", "12345678"));
  assert(profiles.count == 5 && profiles.find("office") == -1);
  assert(profiles.find("home") == 4); // Updating did not delete another profile.
  assert(profiles.remember("  spaced SSID  ", "12345678"));
  assert(profiles.find("  spaced SSID  ") == 0);
  char maxSsid[33], maxPassword[64];
  memset(maxSsid, 's', 32); maxSsid[32] = 0;
  memset(maxPassword, 'p', 63); maxPassword[63] = 0;
  assert(profiles.remember(maxSsid, maxPassword));
  WifiProfiles snapshot = profiles;
  assert(!profiles.remember("123456789012345678901234567890123", "12345678"));
  assert(profiles.count == snapshot.count && strcmp(profiles.entries[0].ssid, snapshot.entries[0].ssid) == 0);
  // Deserialize stored order by visiting JSON entries in reverse order.
  WifiProfiles restored;
  for (size_t i = snapshot.count; i > 0; --i)
    assert(restored.remember(snapshot.entries[i - 1].ssid, snapshot.entries[i - 1].password));
  for (size_t i = 0; i < snapshot.count; ++i) {
    assert(strcmp(restored.entries[i].ssid, snapshot.entries[i].ssid) == 0);
    assert(strcmp(restored.entries[i].password, snapshot.entries[i].password) == 0);
  }
  puts("PASS: profile retention, password update, no duplicates, capacity eviction, exact SSIDs and restore order.");
}
