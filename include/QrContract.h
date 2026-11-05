#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace QrContract {
inline uint32_t timestamp(uint32_t now, uint32_t interval) {
  return interval ? now / interval * interval : 0;
}
inline bool message(char* out, size_t size, const char* did, uint32_t time) {
  size_t length = strlen(did);
  if (!length || length > 32) return false;
  for (size_t i = 0; i < length; ++i)
    if (!((did[i] >= 'a' && did[i] <= 'z') || (did[i] >= '0' && did[i] <= '9') || did[i] == '-')) return false;
  int written = snprintf(out, size, "v1.%s.%lu", did, static_cast<unsigned long>(time));
  return written > 0 && static_cast<size_t>(written) < size;
}
inline bool derToRaw(const uint8_t* der, size_t length, uint8_t raw[64]) {
  if (length < 8 || length > 72 || der[0] != 0x30 || der[1] != length - 2) return false;
  size_t offset = 2;
  memset(raw, 0, 64);
  for (size_t i = 0; i < 2; ++i) {
    if (offset + 2 > length || der[offset++] != 2) return false;
    size_t n = der[offset++];
    if (n < 1 || n > 33 || offset + n > length || (der[offset] & 0x80)) return false;
    const uint8_t* value = der + offset;
    offset += n;
    if (n > 1 && value[0] == 0) {
      if (!(value[1] & 0x80)) return false;
      ++value; --n;
    }
    if (n > 32) return false;
    memcpy(raw + i * 32 + 32 - n, value, n);
  }
  return offset == length;
}
}
