#include "BrandedQr.h"
struct Canvas {
  uint16_t* pixels;
  void writeFillRect(int x, int y, int w, int h, uint16_t color) {
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx)
      if (xx >= 0 && yy >= 0 && xx < 240 && yy < 240) pixels[yy * 240 + xx] = color;
  }
};
extern "C" __declspec(dllexport) int renderPreview(const char* text, uint16_t* pixels) {
  Canvas canvas{pixels};
  return BrandedQr::render(canvas, text);
}
