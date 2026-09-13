#pragma once

#include <Arduino.h>

namespace AntSprite {
constexpr int16_t kBodyWidth = 14;
constexpr int16_t kBodyHeight = 19;
constexpr int16_t kPadding = 2;
constexpr int16_t kSpriteWidth = kBodyWidth + kPadding * 2;   // 18
constexpr int16_t kSpriteHeight = kBodyHeight + kPadding * 2; // 23
constexpr uint8_t kFrameCount = 2;

// Bit x in each row represents source pixel x. These are the approved,
// reduced 14x19 two-frame walking silhouettes used by the HTML prototype.
constexpr uint16_t kRows[kFrameCount][kBodyHeight] = {
    {
        0x0408, 0x0638, 0x03F0, 0x01E0, 0x01E3,
        0x31EE, 0x39FC, 0x0FF0, 0x3FF8, 0x31FC,
        0x03FE, 0x07DB, 0x0DEC, 0x1BF6, 0x33F3,
        0x33F1, 0x03F0, 0x03E0, 0x01E0,
    },
    {
        0x0618, 0x0618, 0x03F0, 0x01E0, 0x31E0,
        0x1DE3, 0x0FE7, 0x03FC, 0x07FF, 0x0FE3,
        0x3FF0, 0x36F8, 0x0DEC, 0x1BF6, 0x33F7,
        0x23F3, 0x03F0, 0x03E0, 0x01E0,
    },
};

inline bool bodyPixel(uint8_t frame, int16_t x, int16_t y) {
  if (frame >= kFrameCount || x < 0 || x >= kBodyWidth || y < 0 || y >= kBodyHeight) {
    return false;
  }
  return (kRows[frame][y] & (uint16_t(1) << x)) != 0;
}

// Returns 0 for transparent, 1 for shadow, 2 for ant body. The shadow is
// baked one pixel down/right before the whole sprite is rotated, matching the
// browser version's cached sprite.
inline uint8_t sample(uint8_t frame, int16_t spriteX, int16_t spriteY) {
  const int16_t bodyX = spriteX - kPadding;
  const int16_t bodyY = spriteY - kPadding;
  if (bodyPixel(frame, bodyX, bodyY)) {
    return 2;
  }
  if (bodyPixel(frame, bodyX - 1, bodyY - 1)) {
    return 1;
  }
  return 0;
}
}  // namespace AntSprite
