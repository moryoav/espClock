#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <esp_heap_caps.h>
#include "esphome/components/display/display.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "flip_glass_assets.h"

namespace flip_glass {
constexpr int kWidth = 480, kHeight = 320;
constexpr int kDigitWidth = flip_glass_assets::kWidth;
constexpr int kDigitHeight = flip_glass_assets::kHeight;
constexpr int kHalf = kDigitHeight / 2, kTop = 84;
constexpr int kReflectionHeight = 100;
constexpr int kBottom = kTop + kDigitHeight + 3 + kReflectionHeight;
constexpr int kPositions[] = {14, 122, 258, 366};
constexpr int kColonX = 190;  // 24-pixel dots centered at x=240 in a 100-pixel cell.
constexpr uint32_t kDurationMs = 1500;
constexpr float kHalfPi = 1.57079632679f;

struct Digit {
  int old_value = -1, value = -1;
  uint32_t started = 0;
  bool flipping = false;

  void set(int next, uint32_t now, bool snap) {
    if (snap || value < 0) {
      old_value = value = next;
      flipping = false;
    } else if (next != value) {
      old_value = value;
      value = next;
      started = now;
      flipping = true;
    }
  }
  float progress(uint32_t now) {
    if (!flipping) return 1.0f;
    const uint32_t elapsed = now - started;  // Correct across millis() wraparound.
    if (elapsed >= kDurationMs) {
      old_value = value;
      flipping = false;
      return 1.0f;
    }
    return float(elapsed) / kDurationMs;
  }
};

inline Digit gDigits[4];
inline uint16_t *gFramebuffer = nullptr;
inline bool gSelected = false, gDirty = true, gHaveTime = false;
inline bool gFullRedraw = true;
inline bool gAllocationFailed = false;
inline uint32_t gTransitionFrames = 0, gTransitionStarted = 0, gTransitionMaxWork = 0;

inline bool selected() { return gSelected; }
inline void set_selected(bool enabled) {
  if (enabled != gSelected) {
    gDirty = true;
    gFullRedraw = true;
    gHaveTime = false;  // Enter at the current time without replaying missed minutes.
    gTransitionFrames = 0;
    for (auto &digit : gDigits) digit = Digit{};
  }
  gSelected = enabled;
}

inline size_t index(int x, int y) { return size_t(x) * 320 + (319 - y); }
inline uint16_t swap16(uint16_t value) { return uint16_t((value << 8) | (value >> 8)); }
inline uint16_t shade(uint16_t color, int level) {
  const unsigned r = ((color >> 11) & 31) * level / 255;
  const unsigned g = ((color >> 5) & 63) * level / 255;
  const unsigned b = (color & 31) * level / 255;
  return uint16_t((std::min(r, 31u) << 11) | (std::min(g, 63u) << 5) | std::min(b, 31u));
}
inline void pixel(int x, int y, uint16_t color) {
  if (x >= 0 && x < kWidth && y >= 0 && y < kHeight)
    gFramebuffer[index(x, y)] = swap16(color);
}
inline uint16_t sample(int glyph, int x, int y) {
  if (glyph < 0 || glyph >= 11 || x < 0 || x >= kDigitWidth || y < 0 || y >= kDigitHeight) return 0;
  return flip_glass_assets::kPixels[(glyph * kDigitHeight + y) * kDigitWidth + x];
}

inline bool begin() {
  if (gFramebuffer) return true;
  if (gAllocationFailed) return false;
  // Keep the 300 KiB framebuffer in PSRAM, leaving internal RAM for BLE/audio.
  gFramebuffer = static_cast<uint16_t *>(heap_caps_malloc(kWidth * kHeight * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!gFramebuffer) {
    gAllocationFailed = true;
    ESP_LOGE("flip_glass", "Unable to allocate PSRAM framebuffer");
    return false;
  }
  ESP_LOGI("flip_glass", "Ready: approved glass artwork, 1500 ms split-flap, 480x320");
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

inline void draw_half(int glyph, int left, bool bottom, int light = 255) {
  const int start = bottom ? kHalf : 0;
  for (int x = 0; x < kDigitWidth; ++x)
    for (int y = start; y < start + kHalf; ++y)
      pixel(left + x, kTop + y, shade(sample(glyph, x, y), light));
}

// Orthographic rotation about the center hinge, with perspective narrowing at
// the free edge. The source is resampled per scanline, never split into assets.
inline void draw_flap(int glyph, int left, bool bottom, float extent, float motion) {
  const int height = std::max(1, int(std::lround(kHalf * extent)));
  for (int row = 0; row < height; ++row) {
    const float distance = (row + 0.5f) / height;
    const int source_distance = std::min(kHalf - 1, int(distance * kHalf));
    const int sy = bottom ? kHalf + source_distance : kHalf - 1 - source_distance;
    const int dy = bottom ? kTop + kHalf + row : kTop + kHalf - 1 - row;
    const float scale = 1.0f - 0.07f * motion * distance;
    const int inset = int(kDigitWidth * (1.0f - scale) * 0.5f);
    // Darken the turning face and sweep a narrow highlight toward its free edge.
    const float band = std::max(0.0f, 1.0f - std::abs(distance - (1.0f - extent)) * 9.0f);
    const int light = int(255 - 100 * motion + 65 * motion * band);
    for (int x = inset; x < kDigitWidth - inset; ++x) {
      const int sx = std::clamp(int((x - kDigitWidth * 0.5f) / scale + kDigitWidth * 0.5f), 0, kDigitWidth - 1);
      pixel(left + x, dy, shade(sample(glyph, sx, sy), light));
    }
  }
}

inline void draw_digit(int position, Digit &digit, uint32_t now) {
  const float p = digit.progress(now);
  if (!digit.flipping) {
    draw_half(digit.value, position, false);
    draw_half(digit.value, position, true);
    return;
  }
  draw_half(digit.value, position, false);
  draw_half(digit.old_value, position, true);
  if (p < 0.5f) {
    const float angle = p * 2.0f * kHalfPi;
    const float motion = std::sin(angle);
    // The falling upper flap casts a soft shadow on the old lower half.
    const int shadow = 2 + int(10 * motion);
    for (int y = 0; y < shadow; ++y)
      for (int x = 0; x < kDigitWidth; ++x)
        pixel(position + x, kTop + kHalf + y,
              shade(sample(digit.old_value, x, kHalf + y), 255 - int(85 * motion * (1.0f - float(y) / shadow))));
    draw_flap(digit.old_value, position, false, std::cos(angle), motion);
  } else {
    const float t = (p - 0.5f) * 2.0f;
    // Cubic easing settles the new lower half gently, with no bounce.
    const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    const float extent = std::sin(eased * kHalfPi);
    draw_flap(digit.value, position, true, extent, 1.0f - extent);
  }
  // The seam exists only while moving. Fade it out as the flap settles.
  const int seam_light = int(255 - 155 * std::sin(p * 3.14159265359f));
  for (int x = 0; x < kDigitWidth; ++x) {
    const int y = kTop + kHalf;
    pixel(position + x, y, shade(swap16(gFramebuffer[index(position + x, y)]), seam_light));
  }
}

inline void draw_reflection(int left = 1, int right = kWidth - 1) {
  constexpr int bottom = kTop + kDigitHeight;
  constexpr int reflection_height = kReflectionHeight;
  // Native panel memory stores each logical column contiguously. Traverse
  // columns first so the blur's three source columns stay in the PSRAM cache.
  // Row-first traversal thrashes that cache and exceeds the animation budget.
  for (int x = left; x < right; ++x) {
    for (int y = 0; y < reflection_height; ++y) {
      const int source_y = bottom - 1 - y * kDigitHeight / reflection_height;
      // Extend the reflection toward the screen bottom with a brighter,
      // gradual linear fade, matching the approved longer-shadow preview.
      const int light = (reflection_height - 1 - y) * 150 / (reflection_height - 1);
      unsigned r = 0, g = 0, b = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const auto c = swap16(gFramebuffer[index(x + dx, std::clamp(source_y + dy, kTop, bottom - 1))]);
          r += (c >> 11) & 31; g += (c >> 5) & 63; b += c & 31;
        }
      }
      pixel(x, bottom + 3 + y, shade(uint16_t(((r / 9) << 11) | ((g / 9) << 5) | (b / 9)), light));
    }
  }
}

inline void draw_frame(uint32_t now, uint8_t mask = 15, bool full = true) {
  if (full) {
    std::fill(gFramebuffer, gFramebuffer + kWidth * kHeight, uint16_t(0));
    mask = 15;
  } else {
    for (int i = 0; i < 4; ++i) {
      if (!(mask & (1 << i))) continue;
      for (int x = kPositions[i] - 1; x <= kPositions[i] + kDigitWidth; ++x) {
        auto *column = gFramebuffer + index(x, kBottom - 1);
        std::fill(column, column + kBottom - kTop, uint16_t(0));
      }
    }
  }
  for (int i = 0; i < 4; ++i)
    if (mask & (1 << i)) draw_digit(kPositions[i], gDigits[i], now);
  // The colon shares a padded atlas cell, but its black padding must not erase
  // the neighboring digits. Only its central 24-pixel column is composited.
  if (full) {
    for (int x = 38; x < 62; ++x)
      for (int y = 0; y < kDigitHeight; ++y)
        pixel(kColonX + x, kTop + y, sample(10, x, y));
    draw_reflection();
  } else {
    for (int i = 0; i < 4; ++i)
      if (mask & (1 << i)) draw_reflection(kPositions[i] - 1, kPositions[i] + kDigitWidth + 1);
  }
}

inline void transfer(esphome::display::Display &display, uint8_t mask, bool full) {
  const auto *pixels = reinterpret_cast<const uint8_t *>(gFramebuffer);
  if (full) {
    display.draw_pixels_at(0, 0, 320, 480, pixels, esphome::display::COLOR_ORDER_RGB,
                           esphome::display::COLOR_BITNESS_565, true);
    return;
  }
  // Send a contiguous native strip for each changing digit. Including its black
  // top/bottom margins avoids a separate QSPI command per scanline, while still
  // reducing a single-digit transfer to 21% of a full screen.
  for (int i = 0; i < 4; ++i) {
    if (!(mask & (1 << i))) continue;
    const int native_y = kPositions[i] - 1;
    display.draw_pixels_at(0, native_y, 320, kDigitWidth + 2, pixels + native_y * 320 * 2,
        esphome::display::COLOR_ORDER_RGB, esphome::display::COLOR_BITNESS_565, true);
  }
}

inline void render(esphome::display::Display &display) {
  if (!begin()) { display.fill(esphome::Color(0, 0, 0)); return; }
  const uint32_t now = esphome::millis();
  const time_t epoch = ::time(nullptr);
  if (epoch > 1700000000) {
    std::tm local{};
    localtime_r(&epoch, &local);
    set_time(local.tm_hour, local.tm_min, now, !gHaveTime);
    gHaveTime = true;
  }
  bool animating = false;
  uint8_t mask = 0;
  for (int i = 0; i < 4; ++i) {
    if (gDigits[i].flipping) mask |= 1 << i;
  }
  animating = mask != 0;
  if (!gDirty && !animating) return;  // No repeated full-screen transfers at rest.
  const uint32_t start = esphome::micros();
  if (animating && gTransitionFrames++ == 0) {
    gTransitionStarted = now;
    gTransitionMaxWork = 0;
  }
  const bool full = gFullRedraw || !animating;
  draw_frame(now, mask, full);
  const uint32_t drawn = esphome::micros();
  transfer(display, mask, full);
  gDirty = false;
  gFullRedraw = false;
  if (animating) {
    gTransitionMaxWork = std::max(gTransitionMaxWork, esphome::micros() - start);
    bool finished = true;
    for (const auto &digit : gDigits) finished &= !digit.flipping;
    if (finished) {
      ESP_LOGI("flip_glass", "Flip complete: %u ms, %u frames, max draw/send %.1f ms",
          unsigned(now - gTransitionStarted), unsigned(gTransitionFrames), gTransitionMaxWork / 1000.0f);
      gTransitionFrames = 0;
    }
  }
  if (!animating) ESP_LOGI("flip_glass", "Frame ready: %02d:%02d, draw/send %.1f/%.1f ms",
      gHaveTime ? gDigits[0].value * 10 + gDigits[1].value : 0,
      gHaveTime ? gDigits[2].value * 10 + gDigits[3].value : 0,
      (drawn - start) / 1000.0f, (esphome::micros() - drawn) / 1000.0f);
}
}  // namespace flip_glass
