#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "car_explosion_assets.h"

#include "esphome/components/display/display.h"
#include "esphome/components/image/image.h"
#include "esphome/core/color.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_system.h>

namespace car_clock {

static const char *const TAG = "car_clock";

namespace Hardware {
constexpr int16_t kLogicalWidth = 480;
constexpr int16_t kLogicalHeight = 320;
constexpr int16_t kPhysicalWidth = 320;
constexpr int16_t kPhysicalHeight = 480;
}  // namespace Hardware

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;
constexpr float kHalfPi = kPi * 0.5f;

struct Rgb { uint8_t r, g, b; };
struct Vec2 { float x, y; };

constexpr Rgb kRoadRgb{18, 21, 23};
constexpr Rgb kRoadEdgeRgb{108, 112, 105};
constexpr Rgb kRoadMarkRgb{163, 154, 111};
constexpr Rgb kShadowRgb{0, 0, 0};
constexpr Rgb kGlassRgb{25, 43, 52};
constexpr Rgb kGlassHighlightRgb{72, 101, 112};
constexpr Rgb kTyreRgb{6, 7, 8};
constexpr Rgb kBumperRgb{62, 67, 69};
constexpr Rgb kHeadlightRgb{255, 238, 158};
constexpr Rgb kTailLightRgb{230, 35, 30};
constexpr Rgb kColonCarRgb{234, 85, 34};
constexpr Rgb kDigitCarRgb[4] = {
    {255, 255, 252}, {250, 250, 248}, {246, 246, 244}, {255, 253, 250},
};

namespace AssetConfig {
constexpr int16_t kSourceWidth = 28;
constexpr int16_t kSourceHeight = 52;
}

inline float clampFloat(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
inline float lerpAngle(float current, float target, float amount) {
  float delta = target - current;
  while (delta > kPi) delta -= kTwoPi;
  while (delta < -kPi) delta += kTwoPi;
  return current + delta * amount;
}
inline float angleDelta(float current, float target) {
  float delta = target - current;
  while (delta > kPi) delta -= kTwoPi;
  while (delta < -kPi) delta += kTwoPi;
  return delta;
}
inline float nearestParallelAngle(float current, float axisAngle) {
  const float forward = axisAngle;
  const float reverse = axisAngle + kPi;
  return fabsf(angleDelta(current, forward)) <= fabsf(angleDelta(current, reverse)) ? forward : reverse;
}
inline uint16_t swap16(uint16_t v) { return static_cast<uint16_t>((v << 8) | (v >> 8)); }
inline uint16_t rgb565(const Rgb &c) {
  return swap16(static_cast<uint16_t>(((c.r & 0xF8u) << 8) | ((c.g & 0xFCu) << 3) | (c.b >> 3)));
}
inline Rgb unpack565(uint16_t color) {
  color = swap16(color);
  const uint8_t r5 = (color >> 11) & 0x1F, g6 = (color >> 5) & 0x3F, b5 = color & 0x1F;
  return {static_cast<uint8_t>((r5 * 255u + 15u) / 31u),
          static_cast<uint8_t>((g6 * 255u + 31u) / 63u),
          static_cast<uint8_t>((b5 * 255u + 15u) / 31u)};
}
inline uint16_t blend565(uint16_t destination, const Rgb &source, uint8_t alpha) {
  if (alpha == 0) return destination;
  if (alpha == 255) return rgb565(source);
  const Rgb dst = unpack565(destination);
  const uint32_t inv = 255u - alpha;
  return rgb565({static_cast<uint8_t>((uint32_t(source.r) * alpha + uint32_t(dst.r) * inv + 127u) / 255u),
                 static_cast<uint8_t>((uint32_t(source.g) * alpha + uint32_t(dst.g) * inv + 127u) / 255u),
                 static_cast<uint8_t>((uint32_t(source.b) * alpha + uint32_t(dst.b) * inv + 127u) / 255u)});
}

uint16_t *gFramebuffer = nullptr;
bool gInitialized = false;
bool gRestartRequested = false;
uint32_t gLastFrameAt = 0;
uint32_t gRandomState = 0xCA4C10C5u;
int8_t gShownHour = -1, gShownMinute = -1;
time_t gFallbackBaseTime = 0;
uint32_t gFallbackBaseMillis = 0;
const esphome::image::Image *gCarSprite = nullptr;
const esphome::image::Image *gCrushedCarSprite = nullptr;

struct FrameTimings {
  uint32_t frames = 0, intervals = 0, lastStart = 0, reportStart = 0;
  uint32_t intervalTotal = 0, intervalMax = 0, simulationTotal = 0, drawingTotal = 0, transferTotal = 0;
  uint32_t workMax = 0, overBudget = 0, movingFrames = 0, movingWork = 0;
  uint16_t histogram[256]{};

  void record(uint32_t start, uint32_t simulationEnd, uint32_t drawingEnd, uint32_t end, bool moving) {
    if (frames != 0) {
      const uint32_t interval = start - lastStart;
      intervalTotal += interval;
      intervalMax = std::max(intervalMax, interval);
      ++histogram[std::min(uint32_t(255), interval / 1000)];
      ++intervals;
    } else {
      reportStart = start;
    }
    lastStart = start;
    ++frames;
    simulationTotal += simulationEnd - start;
    drawingTotal += drawingEnd - simulationEnd;
    transferTotal += end - drawingEnd;
    workMax = std::max(workMax, end - start);
    if (end - start > 42000u) ++overBudget;
    if (moving) { ++movingFrames; movingWork += end - start; }
    if (end - reportStart < 10000000u) return;
    const uint32_t target = (intervals * 95u + 99u) / 100u;
    uint32_t seen = 0;
    uint16_t percentile = 0;
    for (; percentile < 255; ++percentile) {
      seen += histogram[percentile];
      if (seen >= target) break;
    }
    const float denominator = float(frames) * 1000.0f;
    ESP_LOGI(TAG,
             "FPS %.1f | interval p95/max %u/%.1f ms | sim/draw/send %.1f/%.1f/%.1f ms | work max %.1f ms | over-budget %u/%u | moving work %.1f ms (%u frames)",
             intervalTotal ? float(intervals) * 1000000.0f / intervalTotal : 0.0f,
             unsigned(percentile + 1), intervalMax / 1000.0f,
             simulationTotal / denominator, drawingTotal / denominator, transferTotal / denominator,
             workMax / 1000.0f, unsigned(overBudget), unsigned(frames),
             movingFrames ? movingWork / (float(movingFrames) * 1000.0f) : 0.0f, unsigned(movingFrames));
    *this = FrameTimings{};
  }
};
FrameTimings gTimings;

inline uint32_t randomU32() {
  uint32_t x = gRandomState;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  gRandomState = x ? x : 0xCA4C10C5u;
  return gRandomState;
}
inline float random01() { return float((randomU32() >> 8) & 0x00FFFFFFu) / 16777216.0f; }
inline float randomRange(float lo, float hi) { return lo + (hi - lo) * random01(); }

inline size_t framebufferIndex(int16_t x, int16_t y) {
  const int16_t physicalX = Hardware::kPhysicalWidth - y - 1;
  const int16_t physicalY = x;
  return size_t(physicalY) * Hardware::kPhysicalWidth + size_t(physicalX);
}
inline void putPixel(int16_t x, int16_t y, const Rgb &color, uint8_t alpha = 255) {
  if (!gFramebuffer || x < 0 || x >= Hardware::kLogicalWidth || y < 0 || y >= Hardware::kLogicalHeight) return;
  uint16_t &dst = gFramebuffer[framebufferIndex(x, y)];
  dst = blend565(dst, color, alpha);
}
inline void clearFrame(const Rgb &color) {
  std::fill(gFramebuffer, gFramebuffer + size_t(Hardware::kPhysicalWidth) * Hardware::kPhysicalHeight, rgb565(color));
}
void drawHorizontalLine(int16_t y, const Rgb &color, uint8_t alpha) {
  if (y < 0 || y >= Hardware::kLogicalHeight) return;
  for (int16_t x = 0; x < Hardware::kLogicalWidth; ++x) putPixel(x, y, color, alpha);
}
void drawVerticalLine(int16_t x, const Rgb &color, uint8_t alpha) {
  if (x < 0 || x >= Hardware::kLogicalWidth) return;
  for (int16_t y = 0; y < Hardware::kLogicalHeight; ++y) putPixel(x, y, color, alpha);
}

bool pointInsidePolygon(float pointX, float pointY, const float *px, const float *py, uint8_t count) {
  bool inside = false;
  for (uint8_t i = 0, j = count - 1; i < count; j = i++) {
    const bool crosses = ((py[i] > pointY) != (py[j] > pointY)) &&
        (pointX < (px[j] - px[i]) * (pointY - py[i]) / ((py[j] - py[i]) + 0.000001f) + px[i]);
    if (crosses) inside = !inside;
  }
  return inside;
}
void fillPolygon(const float *px, const float *py, uint8_t count, const Rgb &color, uint8_t alpha) {
  if (count < 3 || alpha == 0) return;
  float minX = px[0], maxX = px[0], minY = py[0], maxY = py[0];
  for (uint8_t i = 1; i < count; ++i) {
    minX = std::min(minX, px[i]); maxX = std::max(maxX, px[i]);
    minY = std::min(minY, py[i]); maxY = std::max(maxY, py[i]);
  }
  for (int16_t y = int16_t(floorf(minY)); y <= int16_t(ceilf(maxY)); ++y)
    for (int16_t x = int16_t(floorf(minX)); x <= int16_t(ceilf(maxX)); ++x)
      if (pointInsidePolygon(float(x) + 0.5f, float(y) + 0.5f, px, py, count)) putPixel(x, y, color, alpha);
}
void fillRotatedEllipse(float cx, float cy, float rx, float ry, float rotation, const Rgb &color, uint8_t alpha) {
  if (rx <= 0 || ry <= 0 || alpha == 0) return;
  const float c = cosf(rotation), s = sinf(rotation);
  const float bx = fabsf(c) * rx + fabsf(s) * ry + 1, by = fabsf(s) * rx + fabsf(c) * ry + 1;
  const float irx = 1.0f / (rx * rx), iry = 1.0f / (ry * ry);
  for (int16_t y = int16_t(floorf(cy - by)); y <= int16_t(ceilf(cy + by)); ++y) {
    for (int16_t x = int16_t(floorf(cx - bx)); x <= int16_t(ceilf(cx + bx)); ++x) {
      const float dx = float(x) + 0.5f - cx, dy = float(y) + 0.5f - cy;
      const float lx = c * dx + s * dy, ly = -s * dx + c * dy;
      if (lx * lx * irx + ly * ly * iry <= 1.0f) putPixel(x, y, color, alpha);
    }
  }
}
inline Vec2 rotateLocal(float cx, float cy, float lx, float ly, float rotation) {
  const float c = cosf(rotation), s = sinf(rotation);
  return {cx + c * lx - s * ly, cy + s * lx + c * ly};
}
void fillLocalPolygon(float cx, float cy, float rotation, float scale, const Vec2 *local,
                      uint8_t count, const Rgb &color, uint8_t alpha) {
  if (count > 12) return;
  float px[12], py[12];
  for (uint8_t i = 0; i < count; ++i) {
    const Vec2 p = rotateLocal(cx, cy, local[i].x * scale, local[i].y * scale, rotation);
    px[i] = p.x; py[i] = p.y;
  }
  fillPolygon(px, py, count, color, alpha);
}
void fillLocalRect(float cx, float cy, float rotation, float scale, float lx, float ly,
                   float width, float height, const Rgb &color, uint8_t alpha) {
  const float hw = width * 0.5f, hh = height * 0.5f;
  const Vec2 rect[4] = {{lx - hw, ly - hh}, {lx + hw, ly - hh},
                        {lx + hw, ly + hh}, {lx - hw, ly + hh}};
  fillLocalPolygon(cx, cy, rotation, scale, rect, 4, color, alpha);
}

// Decode the small source images once into internal RAM. Public get_pixel()
// performs format/alpha decoding; calling it four times per output pixel made
// every parked car as expensive to draw as a moving one.
struct DecodedSprite {
  const esphome::image::Image *image = nullptr;
  esphome::Color pixels[AssetConfig::kSourceWidth * AssetConfig::kSourceHeight];

  const esphome::Color *prepare(const esphome::image::Image *source) {
    if (source->get_width() * source->get_height() > int(sizeof(pixels) / sizeof(pixels[0]))) return nullptr;
    if (image != source) {
      for (int y = 0; y < source->get_height(); ++y)
        for (int x = 0; x < source->get_width(); ++x)
          pixels[y * source->get_width() + x] = source->get_pixel(x, y);
      image = source;
    }
    return pixels;
  }
};
DecodedSprite gDecodedCar, gDecodedCrushedCar;

inline uint8_t bilinearChannel(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                               uint32_t w00, uint32_t w10, uint32_t w01, uint32_t w11) {
  return uint8_t((a * w00 + b * w10 + c * w01 + d * w11) >> 16);
}

// One optional raster per car, stored in PSRAM for its lifetime. Parked cars
// reuse their exact RGBA pixels; alpha is still composited over the current road
// and light beams, so moving cars cannot leave trails or overwrite each other.
struct SpriteRasterCache {
  static constexpr size_t kCapacity = 64 * 64;
  esphome::Color *pixels = nullptr;
  bool allocationAttempted = false, valid = false;
  const esphome::image::Image *image = nullptr;
  float x = 0, y = 0, angle = 0, scaleX = 0, scaleY = 0;
  Rgb tint{};
  uint8_t alpha = 0;
  int16_t xMin = 0, xMax = -1, yMin = 0, yMax = -1;

  bool matches(const esphome::image::Image *source, float cx, float cy, float rotation,
               float sx, float sy, const Rgb &color, uint8_t opacity) const {
    return valid && image == source && x == cx && y == cy && angle == rotation &&
        scaleX == sx && scaleY == sy && tint.r == color.r && tint.g == color.g &&
        tint.b == color.b && alpha == opacity;
  }
  bool prepare(int16_t left, int16_t right, int16_t top, int16_t bottom) {
    valid = false;
    const size_t count = size_t(right - left + 1) * size_t(bottom - top + 1);
    if (count > kCapacity) return false;
    if (!allocationAttempted) {
      allocationAttempted = true;
      pixels = static_cast<esphome::Color *>(heap_caps_malloc(kCapacity * sizeof(esphome::Color),
                                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!pixels) return false;  // The direct rasterizer remains the fallback.
    xMin = left; xMax = right; yMin = top; yMax = bottom;
    std::fill(pixels, pixels + count, esphome::Color(0, 0, 0, 0));
    return true;
  }
  void draw() const {
    const esphome::Color *source = pixels;
    for (int16_t px = xMin; px <= xMax; ++px) {
      uint16_t *destination = gFramebuffer + framebufferIndex(px, yMax);
      for (int16_t py = yMax; py >= yMin; --py, ++source, ++destination) {
        if (source->w >= 3) *destination = blend565(*destination, {source->r, source->g, source->b}, source->w);
      }
    }
  }
};

void drawCarSprite(float x, float y, float angle, float scale, const Rgb &tint, uint8_t alpha = 255,
                   const esphome::image::Image *sprite = nullptr,
                   float widthScale = 1.0f, float lengthScale = 1.0f, SpriteRasterCache *cache = nullptr) {
  if (sprite == nullptr) sprite = gCarSprite;
  if (sprite == nullptr || alpha == 0 || scale <= 0 || widthScale <= 0 || lengthScale <= 0) return;
  const int sourceW = sprite->get_width(), sourceH = sprite->get_height();
  if (sourceW <= 0 || sourceH <= 0) return;
  const float rotation = angle + (sourceH >= sourceW ? 0.0f : -kHalfPi);
  const float scaleX = scale * widthScale, scaleY = scale * lengthScale;
  if (cache && cache->matches(sprite, x, y, rotation, scaleX, scaleY, tint, alpha)) {
    cache->draw();
    return;
  }
  const auto *pixels = (sprite == gCrushedCarSprite ? gDecodedCrushedCar : gDecodedCar).prepare(sprite);
  // Keep support for a future image larger than the small decoded cache.
  auto sample = [&](int sx, int sy) -> esphome::Color {
    return pixels ? pixels[sy * sourceW + sx] : sprite->get_pixel(sx, sy);
  };
  const float c = cosf(rotation), s = sinf(rotation);
  const float halfW = (fabsf(c) * sourceW * scaleX + fabsf(s) * sourceH * scaleY) * 0.5f + 1.5f;
  const float halfH = (fabsf(s) * sourceW * scaleX + fabsf(c) * sourceH * scaleY) * 0.5f + 1.5f;
  const int16_t xMin = int16_t(std::max(0.0f, floorf(x - halfW)));
  const int16_t xMax = int16_t(std::min(float(Hardware::kLogicalWidth - 1), ceilf(x + halfW)));
  const int16_t yMin = int16_t(std::max(0.0f, floorf(y - halfH)));
  const int16_t yMax = int16_t(std::min(float(Hardware::kLogicalHeight - 1), ceilf(y + halfH)));
  if (xMin > xMax || yMin > yMax) return;
  if (cache && !cache->prepare(xMin, xMax, yMin, yMax)) cache = nullptr;

  // Walk native display rows: logical y decreases as physical x increases.
  // Contiguous PSRAM writes avoid a 640-byte stride for every adjacent pixel.
  // Increment a Q16 affine transform, keeping division/floor and floating-point
  // interpolation out of the pixel loop. Angles and subpixel motion stay smooth.
  constexpr float fixed = 65536.0f;
  const int32_t stepU = int32_t(lroundf(-s / scaleX * fixed));
  const int32_t stepV = int32_t(lroundf(-c / scaleY * fixed));
  const int32_t maxU = (sourceW - 1) * 65536, maxV = (sourceH - 1) * 65536;
  for (int16_t px = xMin; px <= xMax; ++px) {
    const float dx = float(px) + 0.5f - x, dy = float(yMax) + 0.5f - y;
    int32_t u = int32_t(lroundf(((c * dx + s * dy) / scaleX + (sourceW - 1) * 0.5f) * fixed));
    int32_t v = int32_t(lroundf(((-s * dx + c * dy) / scaleY + (sourceH - 1) * 0.5f) * fixed));
    uint16_t *destination = gFramebuffer + framebufferIndex(px, yMax);
    for (int16_t py = yMax; py >= yMin; --py, ++destination, u += stepU, v += stepV) {
      if (uint32_t(u) > uint32_t(maxU) || uint32_t(v) > uint32_t(maxV)) continue;
      const int x0 = u >> 16, y0 = v >> 16;
      const int x1 = std::min(x0 + 1, sourceW - 1), y1 = std::min(y0 + 1, sourceH - 1);
      const uint32_t fx = (u >> 8) & 255, fy = (v >> 8) & 255;
      const uint32_t w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy);
      const uint32_t w01 = (256 - fx) * fy, w11 = fx * fy;
      const auto c00 = sample(x0, y0), c10 = sample(x1, y0);
      const auto c01 = sample(x0, y1), c11 = sample(x1, y1);
      const uint8_t pixelAlpha = uint32_t(bilinearChannel(c00.w, c10.w, c01.w, c11.w, w00, w10, w01, w11)) * alpha / 255u;
      if (pixelAlpha < 3) continue;
      const Rgb color{
          uint8_t(uint32_t(bilinearChannel(c00.r, c10.r, c01.r, c11.r, w00, w10, w01, w11)) * tint.r / 255u),
          uint8_t(uint32_t(bilinearChannel(c00.g, c10.g, c01.g, c11.g, w00, w10, w01, w11)) * tint.g / 255u),
          uint8_t(uint32_t(bilinearChannel(c00.b, c10.b, c01.b, c11.b, w00, w10, w01, w11)) * tint.b / 255u),
      };
      *destination = blend565(*destination, color, pixelAlpha);
      if (cache) cache->pixels[size_t(px - xMin) * (yMax - yMin + 1) + (yMax - py)] =
          esphome::Color(color.r, color.g, color.b, pixelAlpha);
    }
  }
  if (cache) {
    cache->image = sprite; cache->x = x; cache->y = y; cache->angle = rotation;
    cache->scaleX = scaleX; cache->scaleY = scaleY; cache->tint = tint; cache->alpha = alpha;
    cache->valid = true;
  }
}

namespace SevenSegment {
enum Segment : uint8_t { A=0, B=1, C=2, D=3, E=4, F=5, G=6 };
constexpr uint8_t bit(Segment s) { return static_cast<uint8_t>(1u << static_cast<uint8_t>(s)); }
constexpr uint8_t kDigitMasks[10] = {
    bit(A)|bit(B)|bit(C)|bit(D)|bit(E)|bit(F),
    bit(B)|bit(C),
    bit(A)|bit(B)|bit(G)|bit(E)|bit(D),
    bit(A)|bit(B)|bit(C)|bit(D)|bit(G),
    bit(F)|bit(G)|bit(B)|bit(C),
    bit(A)|bit(F)|bit(G)|bit(C)|bit(D),
    bit(A)|bit(F)|bit(G)|bit(E)|bit(C)|bit(D),
    bit(A)|bit(B)|bit(C),
    bit(A)|bit(B)|bit(C)|bit(D)|bit(E)|bit(F)|bit(G),
    bit(A)|bit(B)|bit(C)|bit(D)|bit(F)|bit(G),
};
}  // namespace SevenSegment

constexpr float kDigitX[4] = {30.0f, 124.0f, 296.0f, 390.0f};
constexpr float kDigitY = 112.0f;
constexpr float kDigitWidth = 60.0f;
constexpr float kDigitHeight = 92.0f;
constexpr float kDigitCarScale = 1.05f;
constexpr float kColonCarScale = 0.56f;
constexpr float kCarMaxSpeed = 155.0f;
constexpr float kArrivalGain = 3.4f;
constexpr float kSteeringGain = 6.4f;
constexpr float kHeadingSmoothing = 0.16f;
constexpr uint8_t kCarsPerDigit = 7;
constexpr uint8_t kDigitCount = 4;
constexpr float kCarYieldRadius = 34.0f;
constexpr float kCarYieldMinGap = 28.0f;

// Light-beam tuning. Beam length is measured from slightly inside the car body,
// so the visible portion beyond the bumper is about half a car long.
constexpr float kLightLateralOffsetFactor = 0.20f;
constexpr float kLightLongitudinalOriginFactor = 0.44f;
constexpr float kLightBeamLengthFactor = 0.58f;
constexpr float kLightBeamNearHalfWidth = 2.8f;
constexpr float kLightBeamFarHalfWidth = 6.2f;
constexpr uint8_t kTailBeamMaxAlpha = 180;
constexpr uint8_t kHeadBeamMaxAlpha = 145;

enum class LightMode : uint8_t { None, Front, Rear };
enum class CrushPhase : uint8_t { None, Squashing, Departing, Replacing };
constexpr uint32_t kSquashImpactMs = 180;
constexpr uint32_t kCrushedHoldMs = 720;
constexpr float kExitMargin = 60.0f;

struct CarTarget { float x, y, angle, garageX, garageY; };

// The approved explosion is already rasterized at both display sizes. Its
// columns match physical framebuffer rows, so this path only clips and blends;
// no image decoding, scaling, rotation, allocation or particle physics per hit.
inline void drawExplosion(float cx, float cy, uint8_t frameIndex, bool colon = false) {
  if (!gFramebuffer || frameIndex >= car_explosion_assets::kFrameCount) return;
  const auto &frame = car_explosion_assets::kFrames[frameIndex + (colon ? 6 : 0)];
  const int left = int(lroundf(cx)) - frame.size / 2;
  const int top = int(lroundf(cy)) - frame.size / 2;
  for (int column = 0; column < frame.size; ++column) {
    const int x = left + column;
    if (x < 0 || x >= Hardware::kLogicalWidth) continue;
    const auto &span = car_explosion_assets::kColumns[frame.columnOffset + column];
    const int first = std::max(int(span.begin), top + frame.size - Hardware::kLogicalHeight);
    const int end = std::min(int(span.begin) + span.count, top + int(frame.size));
    if (first >= end) continue;
    const auto *source = car_explosion_assets::kPixels + span.offset + first - span.begin;
    uint16_t *destination = gFramebuffer + framebufferIndex(x, top + frame.size - 1 - first);
    for (int pixel = first; pixel < end; ++pixel, ++source, ++destination) {
      const uint32_t argb = *source;
      const uint8_t alpha = uint8_t(argb >> 24);
      if (alpha >= 3) *destination = blend565(*destination,
          {uint8_t(argb >> 16), uint8_t(argb >> 8), uint8_t(argb)}, alpha);
    }
  }
}

void drawCarLightBeams(float x, float y, float angle, float scale, LightMode mode) {
  if (gCarSprite == nullptr || mode == LightMode::None) return;

  // angle_ describes the car's conceptual long axis: local -Y is the front,
  // local +Y is the rear. This stays true even if a future sprite is landscape.
  const float sourceW = float(gCarSprite->get_width());
  const float sourceH = float(gCarSprite->get_height());
  const float carWidth = std::min(sourceW, sourceH);
  const float carLength = std::max(sourceW, sourceH);

  const float direction = mode == LightMode::Front ? -1.0f : 1.0f;
  const float originY = direction * carLength * kLightLongitudinalOriginFactor;
  const float beamLength = carLength * kLightBeamLengthFactor;
  const float lightX = carWidth * kLightLateralOffsetFactor;
  const Rgb beamColor = mode == LightMode::Front ? kHeadlightRgb : kTailLightRgb;
  const uint8_t maxAlpha = mode == LightMode::Front ? kHeadBeamMaxAlpha : kTailBeamMaxAlpha;

  // Conservative local bounding rectangle containing both beams.
  const float minLocalX = -lightX - kLightBeamFarHalfWidth - 1.0f;
  const float maxLocalX =  lightX + kLightBeamFarHalfWidth + 1.0f;
  const float endY = originY + direction * beamLength;
  const float minLocalY = std::min(originY, endY) - 1.0f;
  const float maxLocalY = std::max(originY, endY) + 1.0f;

  const Vec2 corners[4] = {
      rotateLocal(x, y, minLocalX * scale, minLocalY * scale, angle),
      rotateLocal(x, y, maxLocalX * scale, minLocalY * scale, angle),
      rotateLocal(x, y, maxLocalX * scale, maxLocalY * scale, angle),
      rotateLocal(x, y, minLocalX * scale, maxLocalY * scale, angle),
  };

  float minX = corners[0].x, maxX = corners[0].x;
  float minY = corners[0].y, maxY = corners[0].y;
  for (uint8_t i = 1; i < 4; ++i) {
    minX = std::min(minX, corners[i].x);
    maxX = std::max(maxX, corners[i].x);
    minY = std::min(minY, corners[i].y);
    maxY = std::max(maxY, corners[i].y);
  }

  const float c = cosf(angle), s = sinf(angle);
  for (int16_t py = int16_t(floorf(minY)); py <= int16_t(ceilf(maxY)); ++py) {
    for (int16_t px = int16_t(floorf(minX)); px <= int16_t(ceilf(maxX)); ++px) {
      // Transform the screen pixel back into conceptual car-local coordinates.
      const float dx = (float(px) + 0.5f - x) / scale;
      const float dy = (float(py) + 0.5f - y) / scale;
      const float localX = c * dx + s * dy;
      const float localY = -s * dx + c * dy;

      const float t = (localY - originY) * direction / beamLength;
      if (t < 0.0f || t > 1.0f) continue;

      const float halfWidth =
          kLightBeamNearHalfWidth +
          (kLightBeamFarHalfWidth - kLightBeamNearHalfWidth) * t;

      float bestLateralFade = 0.0f;
      const float leftDistance = fabsf(localX + lightX);
      if (leftDistance <= halfWidth)
        bestLateralFade = std::max(bestLateralFade, 1.0f - leftDistance / halfWidth);

      const float rightDistance = fabsf(localX - lightX);
      if (rightDistance <= halfWidth)
        bestLateralFade = std::max(bestLateralFade, 1.0f - rightDistance / halfWidth);

      if (bestLateralFade <= 0.0f) continue;

      // Strongest at the lamps, then smoothly decay both lengthwise and sideways.
      float longitudinalFade = 1.0f - t;
      longitudinalFade *= longitudinalFade;
      const float alphaF =
          float(maxAlpha) * longitudinalFade * bestLateralFade;
      const uint8_t alpha =
          static_cast<uint8_t>(clampFloat(alphaF, 0.0f, 255.0f));
      if (alpha >= 2) putPixel(px, py, beamColor, alpha);
    }
  }
}

class Car {
 public:
  void initialize(uint8_t groupIndex, uint8_t segmentIndex, const CarTarget &target) {
    groupIndex_ = groupIndex;
    segmentIndex_ = segmentIndex;
    target_ = target;
    x_ = target.garageX; y_ = target.garageY;
    vx_ = vy_ = 0.0f;
    angle_ = target.angle;
    wantedActive_ = false; parked_ = true; atDigitTarget_ = false; startAt_ = 0;
    startPending_ = false;
    crushPhase_ = CrushPhase::None;
    scale_ = kDigitCarScale;
    lightMode_ = LightMode::None;
    bodyColor_ = kDigitCarRgb[groupIndex % 4];
  }

  void initializeColon(float x, float y, float angle) {
    initialize(0, 0, {x, y, angle, x, angle == 0.0f ? Hardware::kLogicalHeight + 45.0f : -45.0f});
    x_ = x; y_ = y;
    wantedActive_ = parked_ = atDigitTarget_ = true;
    scale_ = kColonCarScale;
    bodyColor_ = kColonCarRgb;
  }

  void setWanted(bool active, uint32_t nowMs, uint32_t delayMs, bool force = false) {
    if (!force && wantedActive_ == active) return;
    wantedActive_ = active;
    // Keep the current crush/exit intact when the clock rolls over. Its eventual
    // replacement follows the latest segment mask, not the numeral at tap time.
    if (damaged()) return;
    startAt_ = nowMs + delayMs;
    startPending_ = delayMs != 0;
    parked_ = false;
  }

  void update(float dt, uint32_t nowMs) {
    if (crushPhase_ == CrushPhase::Squashing) {
      if (uint32_t(nowMs - crushedAt_) < kCrushedHoldMs) return;
      crushPhase_ = CrushPhase::Departing;
    }
    if (crushPhase_ == CrushPhase::Departing) {
      const float speed = std::min(220.0f, 65.0f + float(uint32_t(nowMs - crushedAt_) - kCrushedHoldMs) * 0.22f);
      vx_ = sinf(angle_) * speed; vy_ = -cosf(angle_) * speed;
      x_ += vx_ * dt; y_ += vy_ * dt;
      if (x_ >= -kExitMargin && x_ <= Hardware::kLogicalWidth + kExitMargin &&
          y_ >= -kExitMargin && y_ <= Hardware::kLogicalHeight + kExitMargin) return;
      vx_ = vy_ = 0.0f;
      startPending_ = false;
      if (!wantedActive_) {
        crushPhase_ = CrushPhase::None;
        x_ = target_.garageX; y_ = target_.garageY;
        parked_ = true;
      } else {
        crushPhase_ = CrushPhase::Replacing;
        x_ = replacementEntry_.x; y_ = replacementEntry_.y;
        parked_ = false;
      }
      return;
    }
    if (waiting(nowMs)) return;
    startPending_ = false;
    const float destinationX = wantedActive_ ? target_.x : target_.garageX;
    const float destinationY = wantedActive_ ? target_.y : target_.garageY;
    const float dx = destinationX - x_, dy = destinationY - y_;
    const float distance = hypotf(dx, dy);

    if (distance < 1.6f) {
      x_ = destinationX; y_ = destinationY; vx_ = vy_ = 0.0f; parked_ = true;
      atDigitTarget_ = wantedActive_;
      crushPhase_ = CrushPhase::None;
      if (wantedActive_) angle_ = nearestParallelAngle(angle_, target_.angle);
      return;
    }

    // This is the exact point where actual movement begins. A light remains on
    // during any scheduled departure delay, then turns off on this frame.
    atDigitTarget_ = false;
    parked_ = false;
    float desiredSpeed = std::min(kCarMaxSpeed, std::max(24.0f, distance * kArrivalGain));
    if (distance < 26.0f) desiredSpeed = std::max(8.0f, distance * 3.2f);
    vx_ = dx / std::max(distance, 0.001f) * desiredSpeed;
    vy_ = dy / std::max(distance, 0.001f) * desiredSpeed;

    const float step = std::min(distance, desiredSpeed * dt);
    x_ += dx / distance * step;
    y_ += dy / distance * step;

    const float travelAxis = atan2f(vy_, vx_) + kHalfPi;
    angle_ = nearestParallelAngle(travelAxis, travelAxis);

    const float remainingDx = destinationX - x_;
    const float remainingDy = destinationY - y_;
    const float remainingDistance = hypotf(remainingDx, remainingDy);
    if (wantedActive_ && remainingDistance < 24.0f) {
      angle_ = nearestParallelAngle(angle_, target_.angle);
    }
  }

  void renderLights() const {
    if (damaged() || !atDigitTarget_ || lightMode_ == LightMode::None) return;
    drawCarLightBeams(x_, y_, angle_, scale_, lightMode_);
  }

  void render(uint32_t nowMs) const {
    // Incoming cars stay hidden until their start time. A car waiting to depart
    // remains visible (and lit) until its actual movement begins.
    if (!visible(nowMs)) return;
    const uint32_t age = uint32_t(nowMs - crushedAt_);
    const bool showDamage = damaged() && age >= kSquashImpactMs / 2;
    const float squash = crushPhase_ == CrushPhase::Squashing && age < kSquashImpactMs
        ? sinf(kPi * float(age) / float(kSquashImpactMs)) : 0.0f;
    drawCarSprite(x_, y_, angle_, scale_, showDamage ? Rgb{255, 255, 255} : bodyColor_, 255,
                  showDamage ? gCrushedCarSprite : gCarSprite,
                  1.0f + 0.14f * squash, 1.0f - 0.32f * squash,
                  (parkedActive() || (crushPhase_ == CrushPhase::Squashing && squash == 0.0f)) ? &rasterCache_ : nullptr);
  }

  void setLightMode(LightMode mode) { lightMode_ = mode; }

  int8_t explosionFrame(uint32_t nowMs) const {
    if (!damaged()) return -1;
    const uint32_t age = nowMs - crushedAt_;
    if (age < car_explosion_assets::kImpactDelayMs ||
        age >= car_explosion_assets::kImpactDelayMs + car_explosion_assets::kDurationMs) return -1;
    return int8_t((age - car_explosion_assets::kImpactDelayMs) * car_explosion_assets::kFrameCount /
                  car_explosion_assets::kDurationMs);
  }

  void renderExplosion(uint32_t nowMs) const {
    const int8_t frame = explosionFrame(nowMs);
    if (frame >= 0) drawExplosion(explosionOrigin_.x, explosionOrigin_.y, uint8_t(frame), scale_ == kColonCarScale);
  }

  bool visible(uint32_t nowMs) const {
    if (damaged()) return true;
    if (waiting(nowMs)) return atDigitTarget_;
    return !(!wantedActive_ && parked_ && !atDigitTarget_);
  }
  bool moving(uint32_t nowMs) const {
    if (crushPhase_ == CrushPhase::Squashing) return false;
    return visible(nowMs) && !waiting(nowMs) && !parked_ && !atDigitTarget_;
  }
  bool parkedActive() const { return !damaged() && wantedActive_ && parked_ && atDigitTarget_; }
  bool damaged() const { return crushPhase_ == CrushPhase::Squashing || crushPhase_ == CrushPhase::Departing; }
  CrushPhase crushPhase() const { return crushPhase_; }
  float angle() const { return angle_; }

  float hitScore(float px, float py, uint32_t nowMs) const {
    if (!visible(nowMs) || damaged() || gCarSprite == nullptr) return -1.0f;
    const float dx = px - x_, dy = py - y_;
    const float localX = cosf(angle_) * dx + sinf(angle_) * dy;
    const float localY = -sinf(angle_) * dx + cosf(angle_) * dy;
    const float hw = std::min(gCarSprite->get_width(), gCarSprite->get_height()) * scale_ * 0.5f + 4.0f;
    const float hh = std::max(gCarSprite->get_width(), gCarSprite->get_height()) * scale_ * 0.5f + 4.0f;
    if (fabsf(localX) > hw || fabsf(localY) > hh) return -1.0f;
    return dx * dx + dy * dy;
  }

  bool squash(uint32_t nowMs) {
    if (!visible(nowMs) || damaged() || gCrushedCarSprite == nullptr) return false;
    crushedAt_ = nowMs;
    explosionOrigin_ = {x_, y_};
    crushPhase_ = CrushPhase::Squashing;
    vx_ = vy_ = 0.0f;
    parked_ = atDigitTarget_ = startPending_ = false;
    // Spawn the fresh car beyond the edge behind its original heading. Even
    // the inner horizontal segments enter from outside the visible display.
    const float backX = -sinf(angle_), backY = cosf(angle_);
    float distance = 10000.0f;
    if (fabsf(backX) > 0.001f)
      distance = std::min(distance, ((backX > 0 ? Hardware::kLogicalWidth + kExitMargin : -kExitMargin) - target_.x) / backX);
    if (fabsf(backY) > 0.001f)
      distance = std::min(distance, ((backY > 0 ? Hardware::kLogicalHeight + kExitMargin : -kExitMargin) - target_.y) / backY);
    replacementEntry_ = {target_.x + backX * distance, target_.y + backY * distance};
    return true;
  }
  float x() const { return x_; }
  float y() const { return y_; }
  float vx() const { return vx_; }
  float vy() const { return vy_; }
  void applyDisplacement(float dx, float dy) {
    if (damaged()) return;
    x_ += dx; y_ += dy;
    if (fabsf(dx) > 0.001f || fabsf(dy) > 0.001f) atDigitTarget_ = false;
  }

 private:
  bool waiting(uint32_t nowMs) const { return startPending_ && int32_t(nowMs - startAt_) < 0; }
  uint8_t groupIndex_ = 0, segmentIndex_ = 0;
  CarTarget target_{};
  float x_ = 0, y_ = 0, vx_ = 0, vy_ = 0, angle_ = 0;
  bool wantedActive_ = false, parked_ = true, atDigitTarget_ = false;
  uint32_t startAt_ = 0, crushedAt_ = 0;
  bool startPending_ = false;
  float scale_ = kDigitCarScale;
  CrushPhase crushPhase_ = CrushPhase::None;
  Vec2 replacementEntry_{};
  Vec2 explosionOrigin_{};
  LightMode lightMode_ = LightMode::None;
  Rgb bodyColor_{};
  mutable SpriteRasterCache rasterCache_;
};

class CarDigit {
 public:
  void initialize(uint8_t slotIndex, float x) {
    slotIndex_ = slotIndex;
    currentDigit_ = -1;
    // Geometry is defined in screen space (as the user sees it on the rotated display).
    // The top and bottom horizontal cars now sit much closer to the nearby
    // upper/lower vertical cars. We keep only a small visible separation so cars never touch.
    // Derive the geometry from the actual asset dimensions. This keeps the target
    // layout correct for the portrait car.png and for future replacement sprites.
    const float sourceWidth = gCarSprite != nullptr ? float(gCarSprite->get_width()) : 28.0f;
    const float sourceHeight = gCarSprite != nullptr ? float(gCarSprite->get_height()) : 52.0f;
    const float kScaledCarLengthHalf = (std::max(sourceWidth, sourceHeight) * kDigitCarScale) * 0.5f;
    const float kScaledCarWidthHalf = (std::min(sourceWidth, sourceHeight) * kDigitCarScale) * 0.5f;
    const float kScaledCarHeightHalf = sourceHeight * kDigitCarScale * 0.5f;
    constexpr float kSegmentGap = 1.0f;
    constexpr float kTopBottomGap = 4.0f + kSegmentGap;
    constexpr float kInnerGap = 4.0f + kSegmentGap;
    const float upperY = kDigitY + 8.0f;
    const float middleY = upperY + (kScaledCarLengthHalf + kScaledCarWidthHalf + kInnerGap);
    const float lowerY = middleY + (kScaledCarLengthHalf + kScaledCarWidthHalf + kInnerGap);
    const float topY = upperY - (kScaledCarLengthHalf + kScaledCarWidthHalf + kTopBottomGap);
    const float bottomY = lowerY + (kScaledCarLengthHalf + kScaledCarWidthHalf + kTopBottomGap);
    const float centerX = x + kDigitWidth * 0.5f;
    const float rightX = x + kDigitWidth;
    const float horizontalGarageX[4] = {-62.0f, 216.0f, 264.0f, Hardware::kLogicalWidth + 62.0f};
    const bool enterFromLeft = slotIndex == 0 || slotIndex == 2;
    const float garageX = horizontalGarageX[slotIndex];
    const float horizontalAngle = enterFromLeft ? kHalfPi : -kHalfPi;
    const CarTarget targets[kCarsPerDigit] = {
        {centerX, topY + kScaledCarWidthHalf - 2.0f, horizontalAngle, garageX, topY},
        {rightX, upperY, 0.0f, rightX, -45.0f},
        {rightX, lowerY - kScaledCarHeightHalf, 0.0f, rightX, Hardware::kLogicalHeight + 45.0f},
        {centerX, bottomY - kScaledCarWidthHalf * 2.5f, horizontalAngle, garageX, bottomY},
        {x, lowerY - kScaledCarHeightHalf, 0.0f, x, Hardware::kLogicalHeight + 45.0f},
        {x, upperY, 0.0f, x, -45.0f},
        {centerX, middleY - kScaledCarWidthHalf, horizontalAngle, garageX, middleY},
    };
    for (uint8_t segment = 0; segment < kCarsPerDigit; ++segment) {
      cars_[segment].initialize(slotIndex, segment, targets[segment]);
      cars_[segment].setLightMode(LightMode::None);
    }
  }

  void setDigit(uint8_t digit, uint32_t nowMs, bool force = false) {
    if (digit > 9) return;
    const uint8_t nextMask = SevenSegment::kDigitMasks[digit];
    const uint8_t previousMask = currentDigit_ >= 0
        ? SevenSegment::kDigitMasks[static_cast<uint8_t>(currentDigit_)] : 0;
    if (!force && currentDigit_ == static_cast<int8_t>(digit)) return;

    for (uint8_t segment = 0; segment < kCarsPerDigit; ++segment) {
      const uint8_t mask = static_cast<uint8_t>(1u << segment);
      const bool wasActive = (previousMask & mask) != 0;
      const bool becomesActive = (nextMask & mask) != 0;
      if (!force && wasActive == becomesActive) continue;
      const uint32_t delayMs = becomesActive
          ? (force ? 180u + uint32_t(segment) * 95u : 520u + uint32_t(segment) * 75u)
          : uint32_t(segment) * 55u;
      cars_[segment].setWanted(becomesActive, nowMs, delayMs, force);
    }

    currentDigit_ = static_cast<int8_t>(digit);

    // Lights are tied to the numeral being shown, not to the physical segment cars.
    // Current test mapping:
    //   1 -> B, C
    //   4 -> F, B, C
    //   7 -> C
    for (uint8_t segment = 0; segment < kCarsPerDigit; ++segment) {
      LightMode mode = LightMode::None;

      const bool litForOne =
          digit == 1 && (segment == SevenSegment::B || segment == SevenSegment::C);
      const bool litForFour =
          digit == 4 && (segment == SevenSegment::F ||
                         segment == SevenSegment::B ||
                         segment == SevenSegment::C);
      const bool litForSeven =
          digit == 7 && segment == SevenSegment::C;

      if (litForOne || litForFour || litForSeven)
        mode = LightMode::Rear;

      cars_[segment].setLightMode(mode);
    }
  }

  void update(float dt, uint32_t nowMs) { for (Car &car : cars_) car.update(dt, nowMs); }
  void renderLights() const { for (const Car &car : cars_) car.renderLights(); }
  void render(uint32_t nowMs) const { for (const Car &car : cars_) car.render(nowMs); }
  void renderExplosions(uint32_t nowMs) const { for (const Car &car : cars_) car.renderExplosion(nowMs); }
  Car &car(uint8_t i) { return cars_[i]; }
  const Car &car(uint8_t i) const { return cars_[i]; }

 private:
  uint8_t slotIndex_ = 0;
  int8_t currentDigit_ = -1;
  Car cars_[kCarsPerDigit];
};

CarDigit gDigits[kDigitCount];

Car gColonCars[2];

// Called only for a released tap. The shared gesture recognizer consumes swipes.
inline bool tap(int16_t x, int16_t y) {
  if (!gInitialized || x < 0 || y < 0 || x >= Hardware::kLogicalWidth || y >= Hardware::kLogicalHeight) return false;
  const uint32_t nowMs = esphome::millis();
  Car *best = nullptr;
  float bestScore = 1000000.0f;
  auto consider = [&](Car &car) {
    const float score = car.hitScore(float(x), float(y), nowMs);
    if (score >= 0.0f && score < bestScore) { best = &car; bestScore = score; }
  };
  for (CarDigit &digit : gDigits)
    for (uint8_t i = 0; i < kCarsPerDigit; ++i) consider(digit.car(i));
  for (Car &car : gColonCars) consider(car);
  return best != nullptr && best->squash(nowMs);
}

time_t parseBuildTime() {
  char monthText[4]{};
  int day = 1, year = 2026, hour = 12, minute = 0, second = 0;
  sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  constexpr const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *monthLocation = strstr(months, monthText);
  const int month = monthLocation ? int((monthLocation - months) / 3) : 0;
  tm build{};
  build.tm_year = year - 1900; build.tm_mon = month; build.tm_mday = day;
  build.tm_hour = hour; build.tm_min = minute; build.tm_sec = second; build.tm_isdst = -1;
  return mktime(&build);
}
inline bool systemTimeIsValid() { return ::time(nullptr) > 1700000000; }
inline time_t liveTimeNow(uint32_t nowMs) {
  if (systemTimeIsValid()) return ::time(nullptr);
  return gFallbackBaseTime + static_cast<time_t>((nowMs - gFallbackBaseMillis) / 1000u);
}

void resolveCarYielding(uint32_t nowMs) {
  Car *cars[kDigitCount * kCarsPerDigit];
  size_t count = 0;
  for (uint8_t d = 0; d < kDigitCount; ++d)
    for (uint8_t c = 0; c < kCarsPerDigit; ++c)
      cars[count++] = &gDigits[d].car(c);

  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      Car &a = *cars[i];
      Car &b = *cars[j];
      if (a.damaged() && b.damaged()) continue;
      if (!a.visible(nowMs) || !b.visible(nowMs)) continue;
      const float dx = b.x() - a.x();
      const float dy = b.y() - a.y();
      const float dist = hypotf(dx, dy);
      if (dist > kCarYieldRadius || dist < 0.001f) continue;
      const bool aMoving = a.moving(nowMs);
      const bool bMoving = b.moving(nowMs);
      if (!aMoving && !bMoving) continue;
      const float push = (kCarYieldMinGap - dist) * 0.5f;
      if (push <= 0.0f) continue;
      const float ux = dx / dist;
      const float uy = dy / dist;
      if (aMoving && !bMoving) {
        b.applyDisplacement(ux * push * 1.7f, uy * push * 1.7f);
      } else if (!aMoving && bMoving) {
        a.applyDisplacement(-ux * push * 1.7f, -uy * push * 1.7f);
      } else {
        a.applyDisplacement(-ux * push * 0.35f, -uy * push * 0.35f);
        b.applyDisplacement(ux * push * 0.35f, uy * push * 0.35f);
      }
    }
  }
}

void applyDisplayedTime(time_t timestamp, uint32_t nowMs, bool force = false) {
  tm local{};
  localtime_r(&timestamp, &local);
  if (!force && local.tm_hour == gShownHour && local.tm_min == gShownMinute) return;
  gShownHour = static_cast<int8_t>(local.tm_hour);
  gShownMinute = static_cast<int8_t>(local.tm_min);
  const uint8_t digits[4] = {
      static_cast<uint8_t>(local.tm_hour / 10), static_cast<uint8_t>(local.tm_hour % 10),
      static_cast<uint8_t>(local.tm_min / 10), static_cast<uint8_t>(local.tm_min % 10),
  };
  for (uint8_t i = 0; i < kDigitCount; ++i) gDigits[i].setDigit(digits[i], nowMs, force);
}

void drawCarParkBackground() {
  clearFrame(kRoadRgb);
  drawHorizontalLine(38, kRoadEdgeRgb, 78);
  drawHorizontalLine(Hardware::kLogicalHeight - 39, kRoadEdgeRgb, 78);
  drawVerticalLine(102, kRoadEdgeRgb, 22);
  drawVerticalLine(196, kRoadEdgeRgb, 22);
  drawVerticalLine(284, kRoadEdgeRgb, 22);
  drawVerticalLine(378, kRoadEdgeRgb, 22);
  for (int16_t x = 7; x < Hardware::kLogicalWidth; x += 28) {
    for (int16_t px = x; px < x + 11; ++px) {
      putPixel(px, 26, kRoadMarkRgb, 70);
      putPixel(px, Hardware::kLogicalHeight - 27, kRoadMarkRgb, 70);
    }
  }
}

void initializeCars(uint32_t nowMs) {
  gTimings = FrameTimings{};
  gLastFrameAt = nowMs;
  for (uint8_t i = 0; i < kDigitCount; ++i) gDigits[i].initialize(i, kDigitX[i]);
  gColonCars[0].initializeColon(240.0f, 138.0f, 0.0f);
  gColonCars[1].initializeColon(240.0f, 178.0f, kPi);
  gShownHour = -1; gShownMinute = -1;
  applyDisplayedTime(liveTimeNow(nowMs), nowMs, true);
}

inline bool begin() {
  if (gInitialized) return true;
  const size_t bytes = size_t(Hardware::kPhysicalWidth) * Hardware::kPhysicalHeight * sizeof(uint16_t);
  gFramebuffer = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!gFramebuffer) gFramebuffer = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
  if (!gFramebuffer) {
    ESP_LOGE(TAG, "Unable to allocate %u-byte car-clock framebuffer", static_cast<unsigned>(bytes));
    return false;
  }
  gRandomState ^= esp_random();
  gFallbackBaseTime = parseBuildTime();
  gFallbackBaseMillis = esphome::millis();
  gLastFrameAt = esphome::millis();
  initializeCars(gLastFrameAt);
  drawCarParkBackground();
  gInitialized = true;
  ESP_LOGI(TAG, "Car renderer ready: cached cars, six-frame 128px/72px explosion (2026-09-13)");
  return true;
}

inline void restart_entry_animation() {
  if (!gInitialized) { gRestartRequested = true; return; }
  initializeCars(esphome::millis());
  gRestartRequested = false;
}

inline void render(esphome::display::Display &it, const esphome::image::Image *car_sprite,
                   const esphome::image::Image *crushed_car_sprite) {
  gCarSprite = car_sprite;
  gCrushedCarSprite = crushed_car_sprite;
  if (gCarSprite == nullptr) {
    it.fill(esphome::Color(kRoadRgb.r, kRoadRgb.g, kRoadRgb.b));
    return;
  }
  if (!begin()) { it.fill(esphome::Color(kRoadRgb.r, kRoadRgb.g, kRoadRgb.b)); return; }
  if (gRestartRequested) restart_entry_animation();
  const uint32_t frameStart = esphome::micros();
  const uint32_t nowMs = esphome::millis();
  const float dt = clampFloat(float(nowMs - gLastFrameAt) / 1000.0f, 0.001f, 0.05f);
  gLastFrameAt = nowMs;
  applyDisplayedTime(liveTimeNow(nowMs), nowMs, false);
  for (CarDigit &digit : gDigits) digit.update(dt, nowMs);
  for (Car &car : gColonCars) car.update(dt, nowMs);
  resolveCarYielding(nowMs);
  bool moving = false;
  for (const CarDigit &digit : gDigits)
    for (uint8_t i = 0; i < kCarsPerDigit; ++i)
      moving |= digit.car(i).moving(nowMs) || digit.car(i).damaged();
  for (const Car &car : gColonCars) moving |= car.moving(nowMs) || car.damaged();
  const uint32_t simulationEnd = esphome::micros();
  drawCarParkBackground();

  // Render every beam first, then every car. This guarantees translucent light
  // can illuminate the road but can never be painted over another car body.
  for (const CarDigit &digit : gDigits) digit.renderLights();
  for (const CarDigit &digit : gDigits) digit.render(nowMs);
  gColonCars[0].render(nowMs); gColonCars[1].render(nowMs);
  // Bursts are a final overlay: the larger approved blast can cover nearby cars
  // briefly, and every frame is rebuilt so fading smoke cannot leave trails.
  for (const CarDigit &digit : gDigits) digit.renderExplosions(nowMs);
  for (const Car &car : gColonCars) car.renderExplosion(nowMs);
  const uint32_t drawingEnd = esphome::micros();
  it.draw_pixels_at(0, 0, Hardware::kPhysicalWidth, Hardware::kPhysicalHeight,
                    reinterpret_cast<const uint8_t *>(gFramebuffer),
                    esphome::display::COLOR_ORDER_RGB,
                    esphome::display::COLOR_BITNESS_565, true);
  gTimings.record(frameStart, simulationEnd, drawingEnd, esphome::micros(), moving);
}

}  // namespace car_clock

namespace clock_router {
enum class DisplayMode : uint8_t { Cars, Ants };
inline DisplayMode gMode = DisplayMode::Cars;
inline bool cars_selected() { return gMode == DisplayMode::Cars; }
inline void set_mode(DisplayMode mode) { gMode = mode; }
}  // namespace clock_router
