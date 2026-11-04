#pragma once
#include <qrcode.h>
#include <string.h>
#include "QrLogo.h"

namespace BrandedQr {
constexpr uint16_t ink = 0x7801;
constexpr uint16_t white = 0xffff;
inline int edge(int module, int count) { return (module + 4) * 240 / (count + 8); }

template<class Canvas>
void rounded(Canvas& c, int x, int y, int w, int h, int radius, uint16_t color) {
  for (int row = 0; row < h; ++row) {
    int depth = row < h - 1 - row ? row : h - 1 - row;
    int inset = 0;
    if (depth < radius) {
      int dy = 2 * radius - 1 - 2 * depth;
      while ((2 * radius - 1 - 2 * inset) * (2 * radius - 1 - 2 * inset) + dy * dy > 4 * radius * radius) ++inset;
    }
    c.writeFillRect(x + inset, y + row, w - 2 * inset, 1, color);
  }
}

// Shared by the LCD and native preview test. Keep a four-module quiet zone.
template<class Canvas>
bool render(Canvas& c, const char* text) {
  static uint8_t buffer[1300];
  static constexpr int capacities[] = {280, 310, 338, 382}; // byte mode, H, versions 17-20
  int version = 17;
  while (version <= 20 && strlen(text) > static_cast<size_t>(capacities[version - 17])) ++version;
  if (version > 20 || qrcode_getBufferSize(version) > sizeof(buffer)) return false;
  QRCode qr;
  if (qrcode_initText(&qr, buffer, version, ECC_HIGH, text)) return false;
  int n = qr.size;
  c.writeFillRect(0, 0, 240, 240, white);
  for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
    bool finder = (x < 7 && y < 7) || (x >= n - 7 && y < 7) || (x < 7 && y >= n - 7);
    if (!finder && qrcode_getModule(&qr, x, y))
      c.writeFillRect(edge(x, n), edge(y, n), edge(x + 1, n) - edge(x, n), edge(y + 1, n) - edge(y, n), ink);
  }
  const int corners[3][2] = {{0, 0}, {n - 7, 0}, {0, n - 7}};
  for (const auto& corner : corners) {
    for (int layer = 0; layer < 3; ++layer) {
      int x = corner[0] + layer, y = corner[1] + layer;
      rounded(c, edge(x, n), edge(y, n), edge(corner[0] + 7 - layer, n) - edge(x, n),
              edge(corner[1] + 7 - layer, n) - edge(y, n), 3 - layer, layer == 1 ? white : ink);
    }
  }
  // Larger approved logo; validate the complete composited QR, not only its matrix.
  int start = (n - 27) / 2;
  int left = edge(start, n), right = edge(start + 27, n);
  rounded(c, left, left, right - left, right - left, 3, white);
  int logoLeft = edge(start + 1, n), logoRight = edge(start + 26, n);
  int size = logoRight - logoLeft;
  for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
    c.writeFillRect(logoLeft + x, logoLeft + y, 1, 1, qrLogo[(y * QR_LOGO_SIZE / size) * QR_LOGO_SIZE + x * QR_LOGO_SIZE / size]);
  return true;
}
}
