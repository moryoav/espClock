#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esphome/components/image/image.h"
#include "ants_v2_simulation.h"
#include "ants_v2_assets.h"

// A denser mode-specific simulation shares the legacy framebuffer.
// Sprite artwork and walking cadence are unchanged.
namespace ants_v2 {

static const char *const TAG = "ants_v2";
constexpr uint32_t kFrameBudgetUs = 42000;

enum class PixelLayout : uint8_t { Planar565LE, Interleaved565BE, PublicAPI };

struct Atlas {
  const esphome::image::Image *image = nullptr;
  const uint8_t *data = nullptr;
  const uint8_t *alpha = nullptr;
  uint32_t pixels = 0;
  uint16_t width = 0;
  PixelLayout layout = PixelLayout::PublicAPI;

  uint16_t read(uint32_t index, uint8_t &opacity) const {
    if (layout == PixelLayout::Planar565LE) {
      opacity = alpha[index];
      if (opacity == 0) return 0;
      const uint8_t *p = data + index * 2;
      return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }
    if (layout == PixelLayout::Interleaved565BE) {
      const uint8_t *p = data + index * 3;
      opacity = p[2];
      return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
    }
    const auto color = image->get_pixel(index % width, index / width);
    opacity = color.w;
    return uint16_t(((color.r & 0xF8u) << 8) |
                    ((color.g & 0xFCu) << 3) | (color.b >> 3));
  }

  bool bind(const esphome::image::Image *source, uint8_t bank) {
    if (source == nullptr || source->get_width() != ants_v2_assets::kAtlasWidths[bank] ||
        source->get_height() != ants_v2_assets::kAtlasHeights[bank] ||
        source->get_type() != esphome::image::IMAGE_TYPE_RGB565 ||
        !source->has_transparency()) {
      ESP_LOGE(TAG, "Atlas %u is missing or incompatible; expected generated RGB565 + alpha PNG", bank);
      return false;
    }
    image = source;
    width = source->get_width();
    pixels = uint32_t(width) * source->get_height();
    data = source->get_data_start();
    // ESPHome's RGB565 alpha format changed from interleaved big-endian to
    // little-endian RGB565 plus a separate alpha plane. Verify the fast path
    // against the public decoder before using either representation.
    if (source->get_bpp() == 16) {
      layout = PixelLayout::Planar565LE;
      alpha = data + pixels * 2;
    } else if (source->get_bpp() == 24) {
      layout = PixelLayout::Interleaved565BE;
    }
    for (uint16_t sample = 0; sample < 256; ++sample) {
      const uint32_t index = (uint64_t(sample) * (pixels - 1)) / 255;
      uint8_t opacity;
      const uint16_t value = read(index, opacity);
      const auto expected = source->get_pixel(index % width, index / width);
      const uint16_t expected565 = uint16_t(((expected.r & 0xF8u) << 8) |
                                           ((expected.g & 0xFCu) << 3) | (expected.b >> 3));
      if (opacity != expected.w || (opacity != 0 && value != expected565)) {
        layout = PixelLayout::PublicAPI;
        ESP_LOGW(TAG, "Atlas %u uses an unfamiliar byte layout; using the public pixel decoder", bank);
        break;
      }
    }
    return true;
  }
};

struct Walker {
  float angle = 0.0f;
  float cycle = 0.0f;
  float speed = 0.0f;
  uint32_t replacement_until = 0;
  uint8_t frame = 0;
};

struct Timings {
  uint32_t last_start = 0;
  uint32_t since_report = 0;
  uint32_t frames = 0;
  uint32_t intervals = 0;
  uint32_t interval_total = 0;
  uint32_t interval_max = 0;
  uint32_t simulation_total = 0;
  uint32_t drawing_total = 0;
  uint32_t transfer_total = 0;
  uint32_t work_max = 0;
  uint32_t overruns = 0;
  uint16_t histogram[256]{};

  void record(uint32_t start, uint32_t simulation_end, uint32_t drawing_end, uint32_t end) {
    if (frames != 0) {
      const uint32_t interval = start - last_start;
      interval_total += interval;
      interval_max = std::max(interval_max, interval);
      ++histogram[std::min(uint32_t(255), interval / 1000)];
      ++intervals;
    } else {
      since_report = start;
    }
    last_start = start;
    ++frames;
    simulation_total += simulation_end - start;
    drawing_total += drawing_end - simulation_end;
    transfer_total += end - drawing_end;
    work_max = std::max(work_max, end - start);
    if (end - start > kFrameBudgetUs) ++overruns;
    if (end - since_report < 10000000u) return;

    const uint32_t target = (intervals * 95u + 99u) / 100u;
    uint32_t seen = 0;
    uint16_t percentile = 0;
    for (; percentile < 255; ++percentile) {
      seen += histogram[percentile];
      if (seen >= target) break;
    }
    const float denominator = float(frames) * 1000.0f;
    ESP_LOGI(TAG,
             "FPS %.1f | interval p95/max %u/%.1f ms | sim/draw/send %.1f/%.1f/%.1f ms | work max %.1f ms | over-budget %u/%u | ants %u",
             interval_total ? float(intervals) * 1000000.0f / interval_total : 0.0f,
             unsigned(percentile + 1), interval_max / 1000.0f,
             simulation_total / denominator, drawing_total / denominator, transfer_total / denominator,
             work_max / 1000.0f, unsigned(overruns), unsigned(frames), unsigned(ants_v2_sim::gAllAntCount));
    *this = Timings{};
  }
};

inline Atlas gAtlases[ants_v2_assets::kBanks];
inline Walker gWalkers[ants_v2_sim::kTotalAntCount];
inline Timings gTimings;
inline bool gSelected = false;
inline bool gResetWalkers = true;
inline bool gAtlasesReady = false;
inline bool gAtlasFailure = false;

inline bool selected() { return gSelected; }

inline void set_selected(bool selected) {
  if (selected && !gSelected) {
    gResetWalkers = true;
    gTimings = Timings{};
  }
  gSelected = selected;
}

inline uint16_t blend_wire565(uint16_t destination, uint16_t source, uint8_t alpha) {
  if (alpha == 255) return ants_v2_sim::swap16(source);
  // Preserve full 8-bit edge alpha; no binary cutouts or color-key halos.
  const uint8_t r = (source >> 11) & 31;
  const uint8_t g = (source >> 5) & 63;
  const uint8_t b = source & 31;
  const ants_v2_sim::Rgb color{
      uint8_t((r << 3) | (r >> 2)), uint8_t((g << 2) | (g >> 4)), uint8_t((b << 3) | (b >> 2))};
  return ants_v2_sim::blend565(destination, color, alpha);
}

inline uint8_t direction_index(float angle) {
  const int direction = int(lroundf(angle * (ants_v2_assets::kDirections / ants_v2_sim::kTwoPi)));
  return uint8_t((direction % ants_v2_assets::kDirections + ants_v2_assets::kDirections) %
                 ants_v2_assets::kDirections);
}

inline void draw_sprite(float x, float y, float angle, uint8_t frame, uint8_t bank, uint8_t opacity) {
  const Atlas &atlas = gAtlases[bank];
  const auto &sprite = ants_v2_assets::kSprites[bank][frame * ants_v2_assets::kDirections + direction_index(angle)];
  // Artwork is already rotated into native panel coordinates. Both reads and
  // framebuffer writes run along contiguous rows, including at arbitrary headings.
  const int origin_x = ants_v2_sim::Hardware::kPhysicalWidth - 1 - int(lroundf(y)) + sprite.dx;
  const int origin_y = int(lroundf(x)) + sprite.dy;
  const int left = std::max(0, -origin_x);
  const int top = std::max(0, -origin_y);
  const int right = std::min(int(sprite.width), ants_v2_sim::Hardware::kPhysicalWidth - origin_x);
  const int bottom = std::min(int(sprite.height), ants_v2_sim::Hardware::kPhysicalHeight - origin_y);
  if (left >= right || top >= bottom) return;

  for (int row = top; row < bottom; ++row) {
    uint32_t index = uint32_t(sprite.y + row) * atlas.width + sprite.x + left;
    uint16_t *destination = ants_v2_sim::gFramebuffer +
        (origin_y + row) * ants_v2_sim::Hardware::kPhysicalWidth + origin_x + left;
    for (int column = left; column < right; ++column, ++index, ++destination) {
      uint8_t alpha;
      const uint16_t color = atlas.read(index, alpha);
      if (alpha == 0) continue;
      if (opacity != 255) alpha = (uint16_t(alpha) * opacity + 127u) / 255u;
      if (alpha != 0) *destination = blend_wire565(*destination, color, alpha);
    }
  }
}

inline void update_walker(uint8_t index, float dt, float heading_blend, bool reset) {
  const ants_v2_sim::Ant &ant = *ants_v2_sim::gAllAnts[index];
  Walker &walker = gWalkers[index];
  const float speed = ant.speed();
  float desired = walker.angle;
  if (ant.hasTarget() && (ant.settled() || speed < 3.2f)) {
    desired = ant.target()->angle;
  } else if (speed > 1.5f) {
    desired = atan2f(ant.vy(), ant.vx()) + ants_v2_sim::kHalfPi;
  }
  if (reset || walker.replacement_until != ant.replacementUntil()) {
    walker.angle = desired;
    walker.cycle = fmodf(float(index) * 0.61803398875f, 1.0f);
    walker.speed = speed;
    walker.replacement_until = ant.replacementUntil();
  } else {
    walker.angle = ants_v2_sim::lerpAngle(walker.angle, desired, heading_blend);
    walker.speed += (speed - walker.speed) * std::min(1.0f, dt * 10.0f);
  }

  const bool moving = !ant.settled() && speed > (ant.hasTarget() ? 3.5f : 1.2f);
  if (moving) {
    // At 24 FPS the cap traverses essentially every one of the 12 walking
    // frames. Slow ants step slowly; newly accelerated ants do not phase-jump.
    walker.cycle += std::min(walker.speed / 36.0f, 1.9f) * dt;
    walker.cycle -= floorf(walker.cycle);
  } else if (walker.cycle > 0.0f) {
    // Complete the current stride into the neutral pose instead of snapping.
    walker.cycle += 1.5f * dt;
    if (walker.cycle >= 1.0f) walker.cycle = 0.0f;
  }
  walker.frame = std::min(uint8_t(ants_v2_assets::kFrames - 1),
                          uint8_t(walker.cycle * ants_v2_assets::kFrames));
}

inline void render(esphome::display::Display &display, const esphome::image::Image *gold,
                   const esphome::image::Image *idle, const esphome::image::Image *fire) {
  if (!gAtlasesReady && !gAtlasFailure) {
    gAtlasesReady = gAtlases[0].bind(gold, 0) && gAtlases[1].bind(idle, 1) && gAtlases[2].bind(fire, 2);
    gAtlasFailure = !gAtlasesReady;
    if (gAtlasesReady) {
      ESP_LOGI(TAG, "Ready: %u walking frames, %u directions, 3 shared PNG atlases; 70-ant dense simulation",
               unsigned(ants_v2_assets::kFrames), unsigned(ants_v2_assets::kDirections));
    }
  }
  if (!gAtlasesReady || !ants_v2_sim::begin()) {
    ants_v2_sim::render(display);
    return;
  }
  const uint32_t start = esphome::micros();
  const uint32_t now = esphome::millis();
  const float dt = gResetWalkers ? 0.042f :
      ants_v2_sim::clampFloat(float(now - ants_v2_sim::gLastFrameAt) * 0.001f, 0.001f, 0.05f);
  ants_v2_sim::gLastFrameAt = now;
  ants_v2_sim::applyDisplayedTime(ants_v2_sim::resolveDisplayedTime(now), false);
  for (uint8_t i = 0; i < ants_v2_sim::gAllAntCount; ++i)
    ants_v2_sim::gAllAnts[i]->update(dt, now, ants_v2_sim::gPointer);
  ants_v2_sim::resolveAntOverlaps();
  const float heading_blend = 1.0f - expf(-5.0f * dt);
  for (uint8_t i = 0; i < ants_v2_sim::gAllAntCount; ++i)
    update_walker(i, dt, heading_blend, gResetWalkers);
  gResetWalkers = false;
  const uint32_t simulation_end = esphome::micros();

  ants_v2_sim::clearFrame();
  ants_v2_sim::drawAmbientField(now);
  ants_v2_sim::updateAndRenderBloodSpatters(now);
  for (uint8_t layer = 0; layer < 2; ++layer) {
    for (uint8_t i = 0; i < ants_v2_sim::gAllAntCount; ++i) {
      const auto &ant = *ants_v2_sim::gAllAnts[i];
      if (ant.hasTarget() != (layer == 1)) continue;
      const auto &walker = gWalkers[i];
      const uint8_t bank = ant.isColon() ? 2 : (ant.hasTarget() ? 0 : 1);
      uint8_t opacity = 255;
      if (ant.isColon())
        opacity = uint8_t(214.0f + 32.0f * sinf(float(now) * 0.004f + float(i)));
      draw_sprite(ant.x(), ant.y(), walker.angle, walker.frame, bank, opacity);
    }
  }
  const uint32_t drawing_end = esphome::micros();
  display.draw_pixels_at(0, 0, ants_v2_sim::Hardware::kPhysicalWidth, ants_v2_sim::Hardware::kPhysicalHeight,
                         reinterpret_cast<const uint8_t *>(ants_v2_sim::gFramebuffer),
                         esphome::display::COLOR_ORDER_RGB, esphome::display::COLOR_BITNESS_565, true);
  gTimings.record(start, simulation_end, drawing_end, esphome::micros());
}

}  // namespace ants_v2
