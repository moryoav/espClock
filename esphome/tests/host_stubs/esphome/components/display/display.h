#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <vector>
#include "esphome/core/color.h"
namespace esphome::display {
enum ColorOrder { COLOR_ORDER_RGB };
enum ColorBitness { COLOR_BITNESS_565 };
class Display {
 public:
  unsigned transfers = 0;
  unsigned transferred_pixels = 0;
  std::vector<uint16_t> panel = std::vector<uint16_t>(320 * 480, 0);
  void fill(Color) {}
  void draw_pixels_at(int x, int y, int w, int h, const uint8_t *data, ColorOrder, ColorBitness, bool,
                      int x_offset = 0, int y_offset = 0, int x_pad = 0) {
    assert(x >= 0 && y >= 0 && x + w <= 320 && y + h <= 480);
    const int stride = x_offset + w + x_pad;
    for (int row = 0; row < h; ++row)
      std::memcpy(panel.data() + (y + row) * 320 + x,
                  data + ((y_offset + row) * stride + x_offset) * 2, w * 2);
    ++transfers;
    transferred_pixels += w * h;
  }
};
}
