#pragma once
#include <cstdint>
namespace esphome {
struct Color {
  uint8_t r, g, b, w;
  constexpr Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t w = 255)
      : r(r), g(g), b(b), w(w) {}
};
}
