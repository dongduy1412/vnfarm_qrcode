#pragma once
#include <cstring>
#include <cstdlib>
#include "ServerConfig.h"

namespace ServerOrigin {
// Runtime parser shared by setup validation and the management transport.
// HTTP is restricted to private IPv4; no credentials, paths, query or fragment.
inline bool valid(const char* origin) {
  if (!origin || std::strlen(origin) > 160) return false;
  bool https = std::strncmp(origin, "https://", 8) == 0;
  bool http = std::strncmp(origin, "http://", 7) == 0;
  if (!https && !http) return false;
  const char* host = origin + (https ? 8 : 7);
  const char* colon = std::strchr(host, ':');
  size_t length = colon ? static_cast<size_t>(colon - host) : std::strlen(host);
  if (!length || length > 150) return false;
  char hostname[161]; std::memcpy(hostname, host, length); hostname[length] = 0;
  if (!ServerConfig::validHost(hostname)) return false;
  if (colon) {
    const char* port = colon + 1;
    if (!*port || std::strlen(port) > 5) return false;
    for (const char* p = port; *p; ++p) if (*p < '0' || *p > '9') return false;
    long n = std::strtol(port, nullptr, 10);
    if (n < 1 || n > 65535) return false;
  }
  if (https) return true;
  unsigned parts[4] = {};
  const char* p = hostname;
  for (unsigned i = 0; i < 4; ++i) {
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
      if (++digits > 3) return false;
      parts[i] = parts[i] * 10 + (*p++ - '0');
    }
    if (!digits || parts[i] > 255 || (digits > 1 && *(p - digits) == '0')) return false;
    if (i < 3) { if (*p++ != '.') return false; }
    else if (*p) return false;
  }
  return parts[0] == 10 || (parts[0] == 192 && parts[1] == 168) ||
      (parts[0] == 172 && parts[1] >= 16 && parts[1] <= 31);
}
}
