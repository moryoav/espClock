#pragma once

#include <cstdlib>
#include "flip_glass_renderer.h"
#include "flip_glass_light_assets.h"

namespace flip_glass_light {
// Share the proven time/transition state machine, with independent face state.
using Digit = flip_glass::Digit;
constexpr int kWidth = 480, kHeight = 320;
constexpr int kDigitWidth = 100, kDigitHeight = 128, kHalf = 64, kTop = 84;
constexpr int kReflectionHeight = 100, kBottom = 315;
constexpr int kPositions[] = {14, 122, 258, 366};
constexpr int kColonX = 190;
constexpr uint32_t kDurationMs = 1500;
constexpr float kHalfPi = 1.57079632679f;
inline Digit gDigits[4];
inline uint16_t *gFramebuffer = nullptr;
inline uint32_t *gLayer = nullptr;
inline bool gSelected = false, gDirty = true, gHaveTime = false;
inline bool gFullRedraw = true, gAllocationFailed = false;
inline uint32_t gTransitionFrames = 0, gTransitionStarted = 0, gTransitionMaxWork = 0;

inline size_t index(int x, int y) { return size_t(x) * 320 + 319 - y; }
inline uint16_t swap16(uint16_t c) { return uint16_t((c << 8) | (c >> 8)); }
inline bool selected() { return gSelected; }
inline void set_selected(bool enabled) {
  if (enabled != gSelected) {
    gDirty = gFullRedraw = true;
    gHaveTime = false;
    gTransitionFrames = 0;
    for (auto &digit : gDigits) digit = Digit{};
  }
  gSelected = enabled;
}
inline bool begin() {
  if (gFramebuffer) return true;
  if (gAllocationFailed) return false;
  auto *frame = static_cast<uint16_t *>(heap_caps_malloc(kWidth * kHeight * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  auto *layer = static_cast<uint32_t *>(heap_caps_malloc(kWidth * kDigitHeight * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!frame || !layer) {
    std::free(frame); std::free(layer);
    gAllocationFailed = true;
    ESP_LOGE("flip_glass_light", "Unable to allocate PSRAM buffers");
    return false;
  }
  gFramebuffer = frame; gLayer = layer;
  std::fill(gLayer, gLayer + kWidth * kDigitHeight, uint32_t(0));
  ESP_LOGI("flip_glass_light", "Ready: approved light glass artwork, 1500 ms split-flap, 480x320");
  return true;
}
inline void set_time(int hour, int minute, uint32_t now, bool snap = false) {
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return;
  const int values[] = {hour / 10, hour % 10, minute / 10, minute % 10};
  for (int i = 0; i < 4; ++i) {
    if (gDigits[i].value != values[i] || snap) gDirty = true;
    gDigits[i].set(values[i], now, snap);
  }
}
inline uint32_t sample(int glyph, int x, int y) {
  if (glyph < 0 || glyph >= 11 || x < 0 || x >= 100 || y < 0 || y >= 128) return 0;
  return flip_glass_light_assets::kGlyphsARGB[(glyph * 100 + x) * 128 + y];
}
inline uint32_t over(uint32_t src, uint32_t dst) {
  const unsigned inv = 255 - (src >> 24);
  if (inv == 255) return dst;
  if (inv == 0 || dst == 0) return src;
  uint32_t out = 0;
  for (int shift : {0, 8, 16, 24}) {
    const unsigned value = ((src >> shift) & 255) + ((((dst >> shift) & 255) * inv + 127) / 255);
    out |= std::min(value, 255u) << shift;
  }
  return out;
}
inline void layer_pixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= kWidth || y < 0 || y >= kDigitHeight) return;
  auto &target = gLayer[x * kDigitHeight + y];
  target = over(color, target);
}
inline void composite(int x, int y, uint32_t src) {
  if (x < 0 || x >= kWidth || y < 0 || y >= kHeight || src == 0) return;
  const auto pos = index(x, y);
  const uint16_t dst = swap16(gFramebuffer[pos]);
  const unsigned inv = 255 - (src >> 24);
  const unsigned r = std::min<uint32_t>(255u, ((src >> 16) & 255) + (((dst >> 11) * 255 / 31) * inv + 127) / 255);
  const unsigned g = std::min<uint32_t>(255u, ((src >> 8) & 255) + ((((dst >> 5) & 63) * 255 / 63) * inv + 127) / 255);
  const unsigned b = std::min<uint32_t>(255u, (src & 255) + (((dst & 31) * 255 / 31) * inv + 127) / 255);
  gFramebuffer[pos] = swap16(uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)));
}
inline void draw_half(int glyph, int left, bool bottom) {
  const int start = bottom ? kHalf : 0;
  for (int x = 0; x < kDigitWidth; ++x)
    for (int y = start; y < start + kHalf; ++y)
      layer_pixel(left + x, y, sample(glyph, x, y));
}
inline void draw_flap(int glyph, int left, bool bottom, float extent, float motion) {
  const int height = std::max(1, int(std::lround(kHalf * extent)));
  struct Row { int sy, dy, inset; float scale; } rows[kHalf];
  for (int row = 0; row < height; ++row) {
    const float distance = (row + 0.5f) / height;
    const int sy = bottom ? kHalf + std::min(kHalf - 1, int(distance * kHalf)) : kHalf - 1 - std::min(kHalf - 1, int(distance * kHalf));
    const int dy = bottom ? kHalf + row : kHalf - 1 - row;
    const float scale = 1.0f - 0.07f * motion * distance;
    const int inset = int(kDigitWidth * (1.0f - scale) * 0.5f);
    rows[row] = {sy, dy, inset, scale};
  }
  // The ARGB layer has 512-byte columns. Walking across a row aliases PSRAM
  // cache sets; walk down columns instead, as for the rest of this renderer.
  for (int x = 0; x < kDigitWidth; ++x) {
    for (int row = 0; row < height; ++row) {
      const auto &r = rows[row];
      if (x < r.inset || x >= kDigitWidth - r.inset) continue;
      const int sx = std::clamp(int((x - kDigitWidth * 0.5f) / r.scale + kDigitWidth * 0.5f), 0, kDigitWidth - 1);
      layer_pixel(left + x, r.dy, sample(glyph, sx, r.sy));
    }
  }
}
inline void draw_digit(int left, Digit &digit, uint32_t now) {
  const float p = digit.progress(now);
  if (!digit.flipping || p == 0.0f) {
    const int glyph = digit.flipping ? digit.old_value : digit.value;
    draw_half(glyph, left, false); draw_half(glyph, left, true);
    return;
  }
  draw_half(digit.value, left, false);
  draw_half(digit.old_value, left, true);
  if (p < 0.5f) {
    const float angle = p * 2.0f * kHalfPi;
    draw_flap(digit.old_value, left, false, std::cos(angle), std::sin(angle));
  } else {
    const float t = (p - 0.5f) * 2.0f;
    const float eased = 1.0f - (1.0f-t) * (1.0f-t) * (1.0f-t);
    const float extent = std::sin(eased * kHalfPi);
    draw_flap(digit.value, left, true, extent, 1.0f-extent);
  }
}
inline void draw_reflection(int left, int right) {
  // Blur premultiplied color and alpha together to avoid dark/light fringes.
  // A separable 1:2:1 kernel produces the exact original 3x3 result with only
  // three PSRAM reads per pixel. Pack channel pairs into 16-bit lanes while
  // summing horizontally; maxima are 1020, so lanes cannot overflow.
  struct Channels { uint32_t rb, ag; } horizontal[kReflectionHeight + 2]{};
  for (int x = left; x < right; ++x) {
    for (int y = 0; y < kReflectionHeight; ++y) {
      const int sy = kDigitHeight - 1 - y * kDigitHeight / kReflectionHeight;
      const uint32_t a = x > 0 ? gLayer[(x - 1) * kDigitHeight + sy] : 0;
      const uint32_t b = gLayer[x * kDigitHeight + sy];
      const uint32_t c = x + 1 < kWidth ? gLayer[(x + 1) * kDigitHeight + sy] : 0;
      horizontal[y + 1].rb = (a & 0x00ff00ff) + 2 * (b & 0x00ff00ff) + (c & 0x00ff00ff);
      horizontal[y + 1].ag = ((a >> 8) & 0x00ff00ff) + 2 * ((b >> 8) & 0x00ff00ff) + ((c >> 8) & 0x00ff00ff);
    }
    for (int y = 0; y < kReflectionHeight; ++y) {
      const auto &a = horizontal[y], &b = horizontal[y + 1], &c = horizontal[y + 2];
      const unsigned f0 = y > 0 ? flip_glass_light_assets::kReflectionFade[y - 1] : 0;
      const unsigned f1 = 2 * flip_glass_light_assets::kReflectionFade[y];
      const unsigned f2 = y + 1 < kReflectionHeight ? flip_glass_light_assets::kReflectionFade[y + 1] : 0;
      const unsigned red = ((a.rb >> 16) * f0 + (b.rb >> 16) * f1 + (c.rb >> 16) * f2) / 4080;
      const unsigned blue = ((a.rb & 65535) * f0 + (b.rb & 65535) * f1 + (c.rb & 65535) * f2) / 4080;
      const unsigned alpha = ((a.ag >> 16) * f0 + (b.ag >> 16) * f1 + (c.ag >> 16) * f2) / 4080;
      const unsigned green = ((a.ag & 65535) * f0 + (b.ag & 65535) * f1 + (c.ag & 65535) * f2) / 4080;
      const uint32_t color = (alpha << 24) | (red << 16) | (green << 8) | blue;
      composite(x, kTop + kDigitHeight + 3 + y, color);
    }
  }
}
inline void draw_frame(uint32_t now, uint8_t mask = 15, bool full = true) {
  if (full) {
    std::copy(flip_glass_light_assets::kBackground, flip_glass_light_assets::kBackground + kWidth * kHeight, gFramebuffer);
    std::fill(gLayer, gLayer + kWidth * kDigitHeight, uint32_t(0));
    mask = 15;
  } else {
    for (int i = 0; i < 4; ++i) {
      if (!(mask & (1 << i))) continue;
      const int start = kPositions[i] - 1, end = kPositions[i] + kDigitWidth + 1;
      std::copy(flip_glass_light_assets::kBackground + start * 320, flip_glass_light_assets::kBackground + end * 320, gFramebuffer + start * 320);
      std::fill(gLayer + start * kDigitHeight, gLayer + end * kDigitHeight, uint32_t(0));
    }
  }
  for (int i = 0; i < 4; ++i) if (mask & (1 << i)) flip_glass_light::draw_digit(kPositions[i], gDigits[i], now);
  if (full) { draw_half(10, kColonX, false); draw_half(10, kColonX, true); }
  auto compose_strip = [&](int left, int right) {
    for (int x = left; x < right; ++x)
      for (int y = 0; y < kDigitHeight; ++y)
        composite(x, kTop + y, gLayer[x * kDigitHeight + y]);
    draw_reflection(left, right);
  };
  if (full) compose_strip(0, kWidth);
  else for (int i = 0; i < 4; ++i)
    if (mask & (1 << i)) compose_strip(kPositions[i] - 1, kPositions[i] + kDigitWidth + 1);
}
inline void transfer(esphome::display::Display &display, uint8_t mask, bool full) {
  const auto *pixels = reinterpret_cast<const uint8_t *>(gFramebuffer);
  if (full) {
    display.draw_pixels_at(0, 0, 320, 480, pixels, esphome::display::COLOR_ORDER_RGB, esphome::display::COLOR_BITNESS_565, true);
  } else {
    for (int i = 0; i < 4; ++i) {
      if (!(mask & (1 << i))) continue;
      const int y = kPositions[i] - 1;
      display.draw_pixels_at(0, y, 320, kDigitWidth + 2, pixels + y * 320 * 2,
                            esphome::display::COLOR_ORDER_RGB, esphome::display::COLOR_BITNESS_565, true);
    }
  }
}
inline void render(esphome::display::Display &display) {
  if (!begin()) { display.fill(esphome::Color(240, 244, 248)); return; }
  const uint32_t now = esphome::millis();
  const time_t epoch = ::time(nullptr);
  if (epoch > 1700000000) {
    std::tm local{}; localtime_r(&epoch, &local);
    set_time(local.tm_hour, local.tm_min, now, !gHaveTime);
    gHaveTime = true;
  }
  uint8_t mask = 0;
  for (int i = 0; i < 4; ++i) if (gDigits[i].flipping) mask |= 1 << i;
  const bool animating = mask != 0;
  if (!gDirty && !animating) return;
  const uint32_t start = esphome::micros();
  if (animating && gTransitionFrames++ == 0) { gTransitionStarted = now; gTransitionMaxWork = 0; }
  const bool full = gFullRedraw || !animating;
  draw_frame(now, mask, full);
  transfer(display, mask, full);
  gDirty = gFullRedraw = false;
  if (animating) {
    gTransitionMaxWork = std::max(gTransitionMaxWork, esphome::micros() - start);
    bool finished = true;
    for (const auto &digit : gDigits) finished &= !digit.flipping;
    if (finished) {
      ESP_LOGI("flip_glass_light", "Flip complete: %u ms, %u frames, max draw/send %.1f ms",
               unsigned(now - gTransitionStarted), unsigned(gTransitionFrames), gTransitionMaxWork / 1000.0f);
      gTransitionFrames = 0;
    }
  } else {
    ESP_LOGI("flip_glass_light", "Frame ready: %02d:%02d, draw/send %.1f ms",
             gHaveTime ? gDigits[0].value * 10 + gDigits[1].value : 0,
             gHaveTime ? gDigits[2].value * 10 + gDigits[3].value : 0, (esphome::micros() - start) / 1000.0f);
  }
}
}  // namespace flip_glass_light
