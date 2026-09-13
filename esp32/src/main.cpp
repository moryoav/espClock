#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <time.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "ant_sprite.h"
#include "digit_layouts.h"
#include "hardware_jc3248w535.h"
#include "user_config.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace App {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;
constexpr float kHalfPi = kPi * 0.5f;

constexpr float kDigitY = 82.0f;
constexpr float kDigitWidth = 60.0f;
constexpr float kDigitHeight = 156.0f;
constexpr float kDigitX[4] = {24.0f, 104.0f, 296.0f, 376.0f};

constexpr float kTargetSpeed = 118.0f;
constexpr float kIdleSpeedMin = 10.0f;
constexpr float kIdleSpeedMax = 21.0f;
constexpr float kPointerRadius = 58.0f;
constexpr float kPointerForce = 760.0f;
constexpr uint32_t kDemoIntervalMs = 3000;

constexpr float kActiveAntScale = 1.0f;
constexpr float kIdleAntScale = 0.72f;
constexpr float kSettledAntScale = 1.0f;
constexpr uint32_t kWalkFrameMsMoving = 120;
constexpr uint32_t kWalkFrameMsIdle = 240;
constexpr uint32_t kWalkFrameMsSettled = 540;
constexpr float kHeadingSmoothing = 0.18f;
constexpr float kMicroJitter = 0.12f;

constexpr float kAntHitRadius = 13.0f;
constexpr float kTouchHitRadius = 19.0f;
constexpr uint32_t kBloodLifetimeMs = 5200;
constexpr uint32_t kBloodFadeMs = 1500;
constexpr uint8_t kMaximumBloodSpatters = 18;
constexpr float kSeparationRadius = 20.0f;
constexpr float kSeparationForce = 360.0f;
constexpr float kRespawnClearRadius = 36.0f;
constexpr float kRespawnClearForce = 980.0f;
constexpr float kOverlapResolveDistance = 15.5f;
constexpr uint32_t kReplacementProtectedMs = 2600;

constexpr uint8_t kMaximumAntsPerDigit = DigitLayouts::kMaximumAntsPerDigit;
constexpr uint8_t kDigitGroupCount = 4;
constexpr uint8_t kColonAntCount = 2;
constexpr uint8_t kTotalAntCount = kDigitGroupCount * kMaximumAntsPerDigit + kColonAntCount;
constexpr uint32_t kFrameIntervalMs = 1000UL / ANT_CLOCK_TARGET_FPS;

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

constexpr Rgb kBackgroundRgb{5, 11, 13};
constexpr Rgb kAmbientRgb{145, 176, 157};
constexpr Rgb kActiveAntRgb{240, 220, 151};
constexpr Rgb kIdleAntRgb{121, 151, 136};
constexpr Rgb kFireAntRgb{232, 61, 39};

struct Vec2 {
  float x;
  float y;
};

struct Target {
  float x;
  float y;
  float angle;
  uint16_t key;
};

struct PointerState {
  bool inside = false;
  bool down = false;
  float x = -10000.0f;
  float y = -10000.0f;
};

enum class Palette : uint8_t {
  Active,
  Idle,
  Fire,
};

enum class ClockMode : uint8_t {
  Live,
  Manual,
  Demo,
};

class Ant;
class DigitGroup;

Arduino_ESP32QSPI gBus(Hardware::kLcdCs,
                       Hardware::kLcdSck,
                       Hardware::kLcdD0,
                       Hardware::kLcdD1,
                       Hardware::kLcdD2,
                       Hardware::kLcdD3);
Arduino_AXS15231B gPanel(&gBus,
                         GFX_NOT_DEFINED,
                         0,
                         false,
                         Hardware::kPanelWidth,
                         Hardware::kPanelHeight);
Arduino_Canvas gCanvas(Hardware::kPanelWidth,
                       Hardware::kPanelHeight,
                       &gPanel,
                       0,
                       0,
                       0);
uint16_t* gFramebuffer = nullptr;

PointerState gPointer;
ClockMode gClockMode = ClockMode::Live;
time_t gSimulatedTime = 0;
time_t gFallbackBaseTime = 0;
uint32_t gFallbackBaseMillis = 0;
uint32_t gLastDemoAdvanceAt = 0;
int8_t gShownHour = -1;
int8_t gShownMinute = -1;
uint32_t gLastFrameAt = 0;
uint32_t gLastTouchPollAt = 0;
uint32_t gLastFpsReportAt = 0;
uint32_t gFramesSinceReport = 0;
uint32_t gSquashedCount = 0;

bool gTouchWasPressed = false;
bool gTouchConsumed = false;

bool gWifiStarted = false;
bool gNtpConfigured = false;
bool gTimeSyncReported = false;
uint32_t gLastWifiAttemptAt = 0;

uint32_t gRandomState = 0xA17C10C5u;

inline float clampFloat(float value, float minimum, float maximum) {
  return std::max(minimum, std::min(maximum, value));
}

inline int16_t clampInt(int16_t value, int16_t minimum, int16_t maximum) {
  return std::max(minimum, std::min(maximum, value));
}

inline uint32_t randomU32() {
  uint32_t x = gRandomState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  gRandomState = x ? x : 0xA17C10C5u;
  return gRandomState;
}

inline float random01() {
  return static_cast<float>((randomU32() >> 8) & 0x00FFFFFFu) / 16777216.0f;
}

inline float randomRange(float minimum, float maximum) {
  return minimum + (maximum - minimum) * random01();
}

inline uint8_t randomCount(uint8_t minimumInclusive, uint8_t maximumExclusive) {
  return minimumInclusive + static_cast<uint8_t>(randomU32() % (maximumExclusive - minimumInclusive));
}

inline float lerpAngle(float current, float target, float amount) {
  float delta = target - current;
  while (delta > kPi) delta -= kTwoPi;
  while (delta < -kPi) delta += kTwoPi;
  return current + delta * amount;
}

inline uint16_t rgb565(const Rgb& color) {
  return static_cast<uint16_t>(((color.r & 0xF8u) << 8) |
                               ((color.g & 0xFCu) << 3) |
                               (color.b >> 3));
}

inline Rgb unpack565(uint16_t color) {
  const uint8_t r5 = static_cast<uint8_t>((color >> 11) & 0x1Fu);
  const uint8_t g6 = static_cast<uint8_t>((color >> 5) & 0x3Fu);
  const uint8_t b5 = static_cast<uint8_t>(color & 0x1Fu);
  return Rgb{
      static_cast<uint8_t>((r5 * 255u + 15u) / 31u),
      static_cast<uint8_t>((g6 * 255u + 31u) / 63u),
      static_cast<uint8_t>((b5 * 255u + 15u) / 31u),
  };
}

inline uint16_t blend565(uint16_t destination, const Rgb& source, uint8_t alpha) {
  if (alpha == 0) return destination;
  if (alpha == 255) return rgb565(source);
  const Rgb dst = unpack565(destination);
  const uint32_t sourceAlpha = alpha;
  const uint32_t inverse = 255u - sourceAlpha;
  const Rgb blended{
      static_cast<uint8_t>((uint32_t(source.r) * sourceAlpha + uint32_t(dst.r) * inverse + 127u) / 255u),
      static_cast<uint8_t>((uint32_t(source.g) * sourceAlpha + uint32_t(dst.g) * inverse + 127u) / 255u),
      static_cast<uint8_t>((uint32_t(source.b) * sourceAlpha + uint32_t(dst.b) * inverse + 127u) / 255u),
  };
  return rgb565(blended);
}

// Arduino_Canvas rotation 1 maps logical (x,y) to physical framebuffer index:
// x * logicalHeight + (logicalHeight - 1 - y).
inline size_t framebufferIndex(int16_t x, int16_t y) {
  return static_cast<size_t>(x) * Hardware::kScreenHeight +
         static_cast<size_t>(Hardware::kScreenHeight - 1 - y);
}

inline void putPixel(int16_t x, int16_t y, const Rgb& color, uint8_t alpha = 255) {
  if (!gFramebuffer || x < 0 || x >= Hardware::kScreenWidth ||
      y < 0 || y >= Hardware::kScreenHeight) {
    return;
  }
  uint16_t& destination = gFramebuffer[framebufferIndex(x, y)];
  destination = blend565(destination, color, alpha);
}

inline void clearFrame() {
  std::fill(gFramebuffer,
            gFramebuffer + static_cast<size_t>(Hardware::kPanelWidth) * Hardware::kPanelHeight,
            rgb565(kBackgroundRgb));
}

void drawHorizontalLine(int16_t y, const Rgb& color, uint8_t alpha) {
  if (y < 0 || y >= Hardware::kScreenHeight) return;
  for (int16_t x = 0; x < Hardware::kScreenWidth; ++x) {
    putPixel(x, y, color, alpha);
  }
}

void fillRotatedEllipse(float centerX,
                        float centerY,
                        float radiusX,
                        float radiusY,
                        float rotation,
                        const Rgb& color,
                        uint8_t alpha) {
  if (radiusX <= 0.0f || radiusY <= 0.0f || alpha == 0) return;
  const float cosine = cosf(rotation);
  const float sine = sinf(rotation);
  const float boundX = fabsf(cosine) * radiusX + fabsf(sine) * radiusY + 1.0f;
  const float boundY = fabsf(sine) * radiusX + fabsf(cosine) * radiusY + 1.0f;
  const int16_t minX = static_cast<int16_t>(floorf(centerX - boundX));
  const int16_t maxX = static_cast<int16_t>(ceilf(centerX + boundX));
  const int16_t minY = static_cast<int16_t>(floorf(centerY - boundY));
  const int16_t maxY = static_cast<int16_t>(ceilf(centerY + boundY));
  const float inverseRxSquared = 1.0f / (radiusX * radiusX);
  const float inverseRySquared = 1.0f / (radiusY * radiusY);

  for (int16_t y = minY; y <= maxY; ++y) {
    for (int16_t x = minX; x <= maxX; ++x) {
      const float dx = static_cast<float>(x) + 0.5f - centerX;
      const float dy = static_cast<float>(y) + 0.5f - centerY;
      const float localX = cosine * dx + sine * dy;
      const float localY = -sine * dx + cosine * dy;
      if (localX * localX * inverseRxSquared + localY * localY * inverseRySquared <= 1.0f) {
        putPixel(x, y, color, alpha);
      }
    }
  }
}

bool pointInsidePolygon(float pointX,
                        float pointY,
                        const float* polygonX,
                        const float* polygonY,
                        uint8_t count) {
  bool inside = false;
  for (uint8_t i = 0, j = count - 1; i < count; j = i++) {
    const bool crosses = ((polygonY[i] > pointY) != (polygonY[j] > pointY)) &&
                         (pointX < (polygonX[j] - polygonX[i]) *
                                           (pointY - polygonY[i]) /
                                           ((polygonY[j] - polygonY[i]) + 0.000001f) +
                                       polygonX[i]);
    if (crosses) inside = !inside;
  }
  return inside;
}

void fillPolygon(const float* polygonX,
                 const float* polygonY,
                 uint8_t count,
                 const Rgb& color,
                 uint8_t alpha) {
  if (count < 3 || alpha == 0) return;
  float minX = polygonX[0];
  float maxX = polygonX[0];
  float minY = polygonY[0];
  float maxY = polygonY[0];
  for (uint8_t i = 1; i < count; ++i) {
    minX = std::min(minX, polygonX[i]);
    maxX = std::max(maxX, polygonX[i]);
    minY = std::min(minY, polygonY[i]);
    maxY = std::max(maxY, polygonY[i]);
  }

  for (int16_t y = static_cast<int16_t>(floorf(minY));
       y <= static_cast<int16_t>(ceilf(maxY));
       ++y) {
    for (int16_t x = static_cast<int16_t>(floorf(minX));
         x <= static_cast<int16_t>(ceilf(maxX));
         ++x) {
      if (pointInsidePolygon(static_cast<float>(x) + 0.5f,
                             static_cast<float>(y) + 0.5f,
                             polygonX,
                             polygonY,
                             count)) {
        putPixel(x, y, color, alpha);
      }
    }
  }
}

struct BloodCorePoint {
  float angle;
  float radius;
};

struct BloodLobe {
  float x;
  float y;
  float radiusX;
  float radiusY;
  float rotation;
};

struct BloodDrop {
  float x;
  float y;
  float radius;
  float stretch;
  float rotation;
};

struct BloodSpatter {
  float x = 0.0f;
  float y = 0.0f;
  uint32_t bornAt = 0;
  float rotation = 0.0f;
  BloodCorePoint core[16]{};
  BloodLobe lobes[5]{};
  BloodDrop droplets[15]{};
  uint8_t dropletCount = 0;
};

BloodSpatter gBloodSpatters[kMaximumBloodSpatters];
uint8_t gBloodSpatterCount = 0;

void createBloodSpatter(float x, float y, uint32_t nowMs) {
  BloodSpatter spatter;
  spatter.x = x;
  spatter.y = y;
  spatter.bornAt = nowMs;
  spatter.rotation = randomRange(0.0f, kTwoPi);

  for (uint8_t i = 0; i < 16; ++i) {
    spatter.core[i].angle = static_cast<float>(i) / 16.0f * kTwoPi;
    spatter.core[i].radius = randomRange(4.5f, 8.8f) * ((i % 2) ? 0.76f : 1.0f);
  }

  for (BloodLobe& lobe : spatter.lobes) {
    lobe.x = randomRange(-5.0f, 5.0f);
    lobe.y = randomRange(-4.0f, 4.0f);
    lobe.radiusX = randomRange(2.4f, 5.4f);
    lobe.radiusY = randomRange(1.8f, 4.3f);
    lobe.rotation = randomRange(0.0f, kPi);
  }

  spatter.dropletCount = randomCount(9, 15);
  for (uint8_t i = 0; i < spatter.dropletCount; ++i) {
    const float angle = randomRange(0.0f, kTwoPi);
    const float distance = randomRange(8.0f, 24.0f) * powf(random01(), 0.62f);
    BloodDrop& drop = spatter.droplets[i];
    drop.x = cosf(angle) * distance;
    drop.y = sinf(angle) * distance;
    drop.radius = randomRange(0.8f, 2.2f);
    drop.stretch = randomRange(1.0f, 2.2f);
    drop.rotation = angle + randomRange(-0.35f, 0.35f);
  }

  if (gBloodSpatterCount < kMaximumBloodSpatters) {
    gBloodSpatters[gBloodSpatterCount++] = spatter;
  } else {
    for (uint8_t i = 1; i < kMaximumBloodSpatters; ++i) {
      gBloodSpatters[i - 1] = gBloodSpatters[i];
    }
    gBloodSpatters[kMaximumBloodSpatters - 1] = spatter;
  }
}

void updateAndRenderBloodSpatters(uint32_t nowMs) {
  uint8_t writeIndex = 0;
  for (uint8_t readIndex = 0; readIndex < gBloodSpatterCount; ++readIndex) {
    const uint32_t age = nowMs - gBloodSpatters[readIndex].bornAt;
    if (age < kBloodLifetimeMs) {
      if (writeIndex != readIndex) gBloodSpatters[writeIndex] = gBloodSpatters[readIndex];
      ++writeIndex;
    }
  }
  gBloodSpatterCount = writeIndex;

  for (uint8_t i = 0; i < gBloodSpatterCount; ++i) {
    const BloodSpatter& spatter = gBloodSpatters[i];
    const uint32_t age = nowMs - spatter.bornAt;
    const float appear = std::min(1.0f, static_cast<float>(age) / 90.0f);
    const uint32_t fadeStart = kBloodLifetimeMs - kBloodFadeMs;
    const float fade = age <= fadeStart
                           ? 1.0f
                           : std::max(0.0f,
                                      1.0f - static_cast<float>(age - fadeStart) /
                                                 static_cast<float>(kBloodFadeMs));
    const float dry = std::min(1.0f, static_cast<float>(age) / 2200.0f);
    const Rgb bloodColor{
        static_cast<uint8_t>(205.0f - dry * 75.0f),
        static_cast<uint8_t>(18.0f - dry * 10.0f),
        static_cast<uint8_t>(28.0f - dry * 13.0f),
    };
    const Rgb dropletColor{
        static_cast<uint8_t>(std::min(238, bloodColor.r + 28)),
        static_cast<uint8_t>(std::min(35, bloodColor.g + 7)),
        static_cast<uint8_t>(std::min(42, bloodColor.b + 8)),
    };
    const uint8_t alpha = static_cast<uint8_t>(clampFloat(fade * 255.0f, 0.0f, 255.0f));
    const float cosine = cosf(spatter.rotation);
    const float sine = sinf(spatter.rotation);

    float polygonX[16];
    float polygonY[16];
    for (uint8_t p = 0; p < 16; ++p) {
      const float localX = cosf(spatter.core[p].angle) * spatter.core[p].radius * appear;
      const float localY = sinf(spatter.core[p].angle) * spatter.core[p].radius * appear;
      polygonX[p] = spatter.x + cosine * localX - sine * localY;
      polygonY[p] = spatter.y + sine * localX + cosine * localY;
    }
    fillPolygon(polygonX, polygonY, 16, bloodColor, alpha);

    for (const BloodLobe& lobe : spatter.lobes) {
      const float localX = lobe.x * appear;
      const float localY = lobe.y * appear;
      const float centerX = spatter.x + cosine * localX - sine * localY;
      const float centerY = spatter.y + sine * localX + cosine * localY;
      fillRotatedEllipse(centerX,
                         centerY,
                         lobe.radiusX * appear,
                         lobe.radiusY * appear,
                         spatter.rotation + lobe.rotation,
                         bloodColor,
                         alpha);
    }

    for (uint8_t d = 0; d < spatter.dropletCount; ++d) {
      const BloodDrop& drop = spatter.droplets[d];
      const float localX = drop.x * appear;
      const float localY = drop.y * appear;
      const float centerX = spatter.x + cosine * localX - sine * localY;
      const float centerY = spatter.y + sine * localX + cosine * localY;
      fillRotatedEllipse(centerX,
                         centerY,
                         drop.radius * drop.stretch * appear,
                         drop.radius * appear,
                         spatter.rotation + drop.rotation,
                         dropletColor,
                         alpha);
    }
  }
}

class Ant {
 public:
  void initialize(uint8_t groupIndex,
                  uint8_t antIndex,
                  float idleMinX,
                  float idleMaxX,
                  bool isColon) {
    groupIndex_ = groupIndex;
    antIndex_ = antIndex;
    idleMinX_ = idleMinX;
    idleMaxX_ = idleMaxX;
    isColon_ = isColon;
    const uint8_t idleLane = antIndex % 2;
    idleY_ = idleLane == 0
                 ? randomRange(18.0f, 47.0f)
                 : randomRange(Hardware::kScreenHeight - 47.0f,
                               Hardware::kScreenHeight - 18.0f);
    x_ = randomRange(idleMinX_, idleMaxX_);
    y_ = idleY_ + randomRange(-8.0f, 8.0f);
    vx_ = randomRange(-14.0f, 14.0f);
    vy_ = randomRange(-5.0f, 5.0f);
    target_ = nullptr;
    phase_ = randomRange(0.0f, kTwoPi);
    wanderPhase_ = randomRange(0.0f, kTwoPi);
    idleDirection_ = random01() < 0.5f ? -1 : 1;
    idleSpeed_ = randomRange(kIdleSpeedMin, kIdleSpeedMax);
    settled_ = false;
    angle_ = idleDirection_ > 0 ? kHalfPi : -kHalfPi;
    lastAngle_ = angle_;
    isReplacement_ = false;
    replacementUntil_ = 0;
  }

  void update(float dt, uint32_t nowMs, const PointerState& pointer);
  void render(uint32_t nowMs) const;
  void keepInsideCanvas();
  void keepInsideIdleArea();
  void addImpulse(float vx, float vy) {
    vx_ += vx;
    vy_ += vy;
    settled_ = false;
  }
  void respawnReplacement(uint32_t nowMs);

  float hitRadius(float extraRadius = 0.0f) const {
    const float scale = target_ ? kActiveAntScale : kIdleAntScale;
    return std::max(kAntHitRadius, AntSprite::kBodyHeight * scale * 0.58f) + extraRadius;
  }

  float x() const { return x_; }
  float y() const { return y_; }
  float vx() const { return vx_; }
  float vy() const { return vy_; }
  float speed() const { return hypotf(vx_, vy_); }
  bool hasTarget() const { return target_ != nullptr; }
  bool settled() const { return settled_; }
  bool isColon() const { return isColon_; }
  bool isReplacement() const { return isReplacement_; }
  uint32_t replacementUntil() const { return replacementUntil_; }
  const Target* target() const { return target_; }
  void setTarget(const Target* target) {
    target_ = target;
    settled_ = false;
  }

  void displace(float dx, float dy) {
    x_ += dx;
    y_ += dy;
  }
  void changeVelocity(float dvx, float dvy) {
    vx_ += dvx;
    vy_ += dvy;
  }

 private:
  void updateHeading(uint32_t nowMs);

  uint8_t groupIndex_ = 0;
  uint8_t antIndex_ = 0;
  float idleMinX_ = 0.0f;
  float idleMaxX_ = 0.0f;
  bool isColon_ = false;
  float idleY_ = 0.0f;
  float x_ = 0.0f;
  float y_ = 0.0f;
  float vx_ = 0.0f;
  float vy_ = 0.0f;
  const Target* target_ = nullptr;
  float phase_ = 0.0f;
  float wanderPhase_ = 0.0f;
  int8_t idleDirection_ = 1;
  float idleSpeed_ = 0.0f;
  bool settled_ = false;
  float angle_ = 0.0f;
  float lastAngle_ = 0.0f;
  bool isReplacement_ = false;
  uint32_t replacementUntil_ = 0;

  friend Vec2 computeAntSeparation(const Ant& currentAnt, uint32_t nowMs);
  friend void resolveAntOverlaps();
};

class DigitGroup {
 public:
  void initialize(uint8_t slotIndex, float x) {
    slotIndex_ = slotIndex;
    x_ = x;
    currentDigit_ = -1;

    for (uint8_t digit = 0; digit <= 9; ++digit) {
      const DigitLayouts::Definition& definition = DigitLayouts::kDigits[digit];
      targetCounts_[digit] = definition.count;
      for (uint8_t i = 0; i < definition.count; ++i) {
        const DigitLayouts::Point& point = definition.points[i];
        targets_[digit][i] = Target{
            x_ + point.x * kDigitWidth,
            kDigitY + point.y * kDigitHeight,
            point.horizontal ? kHalfPi : 0.0f,
            static_cast<uint16_t>((slotIndex_ << 12) | (digit << 8) | i),
        };
      }
    }

    const float idleMinX = std::max(10.0f, x_ - 24.0f);
    const float idleMaxX = std::min(static_cast<float>(Hardware::kScreenWidth - 10),
                                    x_ + kDigitWidth + 24.0f);
    for (uint8_t i = 0; i < kMaximumAntsPerDigit; ++i) {
      ants_[i].initialize(slotIndex_, i, idleMinX, idleMaxX, false);
    }
  }

  void setDigit(uint8_t digit, bool force = false) {
    if (digit > 9) return;
    if (!force && currentDigit_ == static_cast<int8_t>(digit)) return;
    currentDigit_ = static_cast<int8_t>(digit);
    assignTargets(targets_[digit], targetCounts_[digit]);
  }

  Ant& ant(uint8_t index) { return ants_[index]; }

 private:
  void assignTargets(Target* targets, uint8_t targetCount) {
    bool assigned[kMaximumAntsPerDigit]{};
    bool targetHandled[kMaximumAntsPerDigit]{};

    // Preserve exact targets when the key still exists.
    for (uint8_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
      for (uint8_t antIndex = 0; antIndex < kMaximumAntsPerDigit; ++antIndex) {
        if (assigned[antIndex]) continue;
        const Target* current = ants_[antIndex].target();
        if (current && current->key == targets[targetIndex].key) {
          ants_[antIndex].setTarget(&targets[targetIndex]);
          assigned[antIndex] = true;
          targetHandled[targetIndex] = true;
          break;
        }
      }
    }

    uint8_t availableAnts[kMaximumAntsPerDigit];
    uint8_t availableCount = 0;
    for (uint8_t antIndex = 0; antIndex < kMaximumAntsPerDigit; ++antIndex) {
      if (!assigned[antIndex]) {
        ants_[antIndex].setTarget(nullptr);
        availableAnts[availableCount++] = antIndex;
      }
    }

    uint8_t remainingTargets[kMaximumAntsPerDigit];
    uint8_t remainingCount = 0;
    for (uint8_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
      if (!targetHandled[targetIndex]) remainingTargets[remainingCount++] = targetIndex;
    }

    // Farthest-first greedy matching: the most difficult remaining target gets
    // its nearest available ant first, avoiding one very long stranded trip.
    while (remainingCount > 0 && availableCount > 0) {
      uint8_t selectedRemainingIndex = 0;
      uint8_t selectedAvailableIndex = 0;
      float largestNearestDistance = -1.0f;

      for (uint8_t ri = 0; ri < remainingCount; ++ri) {
        const Target& target = targets[remainingTargets[ri]];
        uint8_t nearestAvailableIndex = 0;
        float nearestDistance = INFINITY;
        for (uint8_t ai = 0; ai < availableCount; ++ai) {
          const Ant& ant = ants_[availableAnts[ai]];
          const float dx = ant.x() - target.x;
          const float dy = ant.y() - target.y;
          const float distanceSquared = dx * dx + dy * dy;
          if (distanceSquared < nearestDistance) {
            nearestDistance = distanceSquared;
            nearestAvailableIndex = ai;
          }
        }
        if (nearestDistance > largestNearestDistance) {
          largestNearestDistance = nearestDistance;
          selectedRemainingIndex = ri;
          selectedAvailableIndex = nearestAvailableIndex;
        }
      }

      const uint8_t targetIndex = remainingTargets[selectedRemainingIndex];
      const uint8_t antIndex = availableAnts[selectedAvailableIndex];
      ants_[antIndex].setTarget(&targets[targetIndex]);

      remainingTargets[selectedRemainingIndex] = remainingTargets[remainingCount - 1];
      --remainingCount;
      availableAnts[selectedAvailableIndex] = availableAnts[availableCount - 1];
      --availableCount;
    }
  }

  uint8_t slotIndex_ = 0;
  float x_ = 0.0f;
  int8_t currentDigit_ = -1;
  Target targets_[10][kMaximumAntsPerDigit]{};
  uint8_t targetCounts_[10]{};
  Ant ants_[kMaximumAntsPerDigit];
};

DigitGroup gDigitGroups[kDigitGroupCount];
Ant gColonAnts[kColonAntCount];
Target gColonTargets[kColonAntCount] = {
    {240.0f, 128.0f, kHalfPi, 0xF000},
    {240.0f, 192.0f, -kHalfPi, 0xF001},
};
Ant* gAllAnts[kTotalAntCount]{};
uint8_t gAllAntCount = 0;

Vec2 computeAntSeparation(const Ant& currentAnt, uint32_t nowMs) {
  Vec2 acceleration{0.0f, 0.0f};
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    const Ant& other = *gAllAnts[i];
    if (&other == &currentAnt) continue;

    const float dx = currentAnt.x_ - other.x_;
    const float dy = currentAnt.y_ - other.y_;
    const float distance = hypotf(dx, dy);
    if (distance < 0.001f) continue;

    const bool otherProtected = other.isReplacement_ && nowMs < other.replacementUntil_;
    const bool currentProtected = currentAnt.isReplacement_ && nowMs < currentAnt.replacementUntil_;
    const float radius = (otherProtected || currentProtected)
                             ? kRespawnClearRadius
                             : kSeparationRadius;
    if (distance >= radius) continue;

    const float falloff = 1.0f - distance / radius;
    float strength = kSeparationForce * falloff * falloff;
    if (otherProtected && !currentProtected) {
      strength += kRespawnClearForce * falloff;
    } else if (currentProtected && !otherProtected) {
      strength *= 0.35f;
    }

    acceleration.x += dx / distance * strength;
    acceleration.y += dy / distance * strength;
  }
  return acceleration;
}

void Ant::update(float dt, uint32_t nowMs, const PointerState& pointer) {
  float ax = 0.0f;
  float ay = 0.0f;

  if (target_) {
    const float seconds = static_cast<float>(nowMs) * 0.001f;
    const float microX = sinf(seconds * 1.7f + phase_) * kMicroJitter;
    const float microY = cosf(seconds * 1.3f + phase_ * 1.23f) * kMicroJitter;
    const float dx = target_->x + microX - x_;
    const float dy = target_->y + microY - y_;
    const float distance = hypotf(dx, dy);
    settled_ = distance < 2.4f && hypotf(vx_, vy_) < 6.0f;
    if (distance > 0.0001f) {
      const float desiredSpeed = std::min(kTargetSpeed, distance * 4.2f);
      ax += (dx / distance * desiredSpeed - vx_) * 7.2f;
      ay += (dy / distance * desiredSpeed - vy_) * 7.2f;
    }
  } else {
    settled_ = false;
    const float wave = sinf(static_cast<float>(nowMs) * 0.00075f + wanderPhase_);
    const float desiredVx = idleDirection_ * idleSpeed_;
    const float desiredVy = (idleY_ - y_) * 1.12f + wave * 4.5f;
    ax += (desiredVx - vx_) * 2.0f;
    ay += (desiredVy - vy_) * 2.0f;
    if (x_ < idleMinX_ + 3.0f) idleDirection_ = 1;
    if (x_ > idleMaxX_ - 3.0f) idleDirection_ = -1;
  }

  if (pointer.inside) {
    const float dx = x_ - pointer.x;
    const float dy = y_ - pointer.y;
    const float distance = hypotf(dx, dy);
    const float radius = pointer.down ? kPointerRadius * 1.28f : kPointerRadius;
    if (distance > 0.001f && distance < radius) {
      const float falloff = 1.0f - distance / radius;
      const float strength = kPointerForce * falloff * falloff * (pointer.down ? 1.55f : 1.0f);
      ax += dx / distance * strength;
      ay += dy / distance * strength;
    }
  }

  const Vec2 separation = computeAntSeparation(*this, nowMs);
  ax += separation.x;
  ay += separation.y;

  vx_ += ax * dt;
  vy_ += ay * dt;
  const float currentSpeed = hypotf(vx_, vy_);
  const float maximumSpeed = target_ ? 205.0f : 48.0f;
  if (currentSpeed > maximumSpeed) {
    vx_ = vx_ / currentSpeed * maximumSpeed;
    vy_ = vy_ / currentSpeed * maximumSpeed;
  }

  const float drag = powf(target_ ? 0.992f : 0.998f, dt * 60.0f);
  vx_ *= drag;
  vy_ *= drag;
  x_ += vx_ * dt;
  y_ += vy_ * dt;
  if (target_) keepInsideCanvas();
  else keepInsideIdleArea();
  updateHeading(nowMs);

  if (isReplacement_) {
    const bool closeEnough = target_ ? hypotf(target_->x - x_, target_->y - y_) < 20.0f : true;
    if (nowMs >= replacementUntil_ || closeEnough) isReplacement_ = false;
  }
}

void Ant::updateHeading(uint32_t nowMs) {
  float desiredAngle = lastAngle_;
  const float currentSpeed = hypotf(vx_, vy_);
  if (target_ && (settled_ || currentSpeed < 3.2f)) {
    desiredAngle = target_->angle;
  } else if (currentSpeed > 1.5f) {
    desiredAngle = atan2f(vy_, vx_) + kHalfPi;
  } else if (!target_) {
    desiredAngle = (idleDirection_ > 0 ? kHalfPi : -kHalfPi) +
                   sinf(wanderPhase_ + static_cast<float>(nowMs) * 0.001f) * 0.1f;
  }
  angle_ = lerpAngle(lastAngle_, desiredAngle, kHeadingSmoothing);
  lastAngle_ = angle_;
}

void Ant::keepInsideCanvas() {
  constexpr float margin = 3.0f;
  if (x_ < margin) {
    x_ = margin;
    vx_ = fabsf(vx_) * 0.55f;
  } else if (x_ > Hardware::kScreenWidth - margin) {
    x_ = Hardware::kScreenWidth - margin;
    vx_ = -fabsf(vx_) * 0.55f;
  }
  if (y_ < margin) {
    y_ = margin;
    vy_ = fabsf(vy_) * 0.55f;
  } else if (y_ > Hardware::kScreenHeight - margin) {
    y_ = Hardware::kScreenHeight - margin;
    vy_ = -fabsf(vy_) * 0.55f;
  }
}

void Ant::keepInsideIdleArea() {
  if (x_ < idleMinX_ - 8.0f) {
    x_ = idleMinX_ - 8.0f;
    idleDirection_ = 1;
    vx_ = fabsf(vx_);
  } else if (x_ > idleMaxX_ + 8.0f) {
    x_ = idleMaxX_ + 8.0f;
    idleDirection_ = -1;
    vx_ = -fabsf(vx_);
  }
  y_ = clampFloat(y_, 6.0f, Hardware::kScreenHeight - 6.0f);
}

void drawAntSprite(float x,
                   float y,
                   float angle,
                   uint8_t frameIndex,
                   float scale,
                   Palette palette,
                   uint8_t globalAlpha) {
  const Rgb* bodyColor = &kActiveAntRgb;
  uint8_t cachedShadowAlpha = 56;
  if (palette == Palette::Idle) {
    bodyColor = &kIdleAntRgb;
    cachedShadowAlpha = 31;
  } else if (palette == Palette::Fire) {
    bodyColor = &kFireAntRgb;
    cachedShadowAlpha = 64;
  }

  const float cosine = cosf(angle);
  const float sine = sinf(angle);
  const float halfWidth = AntSprite::kSpriteWidth * scale * 0.5f;
  const float halfHeight = AntSprite::kSpriteHeight * scale * 0.5f;
  const int16_t boundX = static_cast<int16_t>(ceilf(fabsf(cosine) * halfWidth +
                                                    fabsf(sine) * halfHeight)) + 1;
  const int16_t boundY = static_cast<int16_t>(ceilf(fabsf(sine) * halfWidth +
                                                    fabsf(cosine) * halfHeight)) + 1;
  const int16_t centerX = static_cast<int16_t>(lroundf(x));
  const int16_t centerY = static_cast<int16_t>(lroundf(y));
  const uint8_t shadowAlpha = static_cast<uint8_t>((cachedShadowAlpha * globalAlpha + 127u) / 255u);

  for (int16_t dy = -boundY; dy <= boundY; ++dy) {
    for (int16_t dx = -boundX; dx <= boundX; ++dx) {
      const float localX = cosine * dx + sine * dy;
      const float localY = -sine * dx + cosine * dy;
      const int16_t sourceX = static_cast<int16_t>(floorf(localX / scale +
                                                          AntSprite::kSpriteWidth * 0.5f));
      const int16_t sourceY = static_cast<int16_t>(floorf(localY / scale +
                                                          AntSprite::kSpriteHeight * 0.5f));
      const uint8_t sample = AntSprite::sample(frameIndex % AntSprite::kFrameCount,
                                                sourceX,
                                                sourceY);
      if (sample == 2) {
        putPixel(centerX + dx, centerY + dy, *bodyColor, globalAlpha);
      } else if (sample == 1) {
        putPixel(centerX + dx, centerY + dy, Rgb{0, 0, 0}, shadowAlpha);
      }
    }
  }
}

void Ant::render(uint32_t nowMs) const {
  uint8_t alpha;
  float scale;
  Palette palette;
  if (target_) {
    alpha = settled_ ? 246 : 230;
    scale = settled_ ? kSettledAntScale : kActiveAntScale;
    palette = isColon_ ? Palette::Fire : Palette::Active;
    if (isColon_) {
      const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(nowMs) * 0.004f + phase_);
      alpha = static_cast<uint8_t>(182.0f + pulse * 64.0f);
      scale += pulse * 0.04f;
    }
  } else {
    alpha = 120;
    scale = kIdleAntScale;
    palette = Palette::Idle;
  }

  const float currentSpeed = hypotf(vx_, vy_);
  const uint32_t interval = target_
                                ? (settled_ ? kWalkFrameMsSettled : kWalkFrameMsMoving)
                                : kWalkFrameMsIdle;
  const uint8_t frameOffset = static_cast<uint8_t>(floorf((phase_ / kTwoPi) * 2.0f));
  const bool moving = currentSpeed > (target_ ? 3.5f : 1.2f);
  const uint8_t frameIndex = moving
                                 ? static_cast<uint8_t>(((nowMs + frameOffset * interval / 2u) /
                                                         interval) %
                                                        2u)
                                 : 0;

  drawAntSprite(x_, y_, angle_, frameIndex, scale, palette, alpha);
}

const char* farthestSpawnEdge(float x, float y) {
  struct EdgeDistance {
    const char* edge;
    float distance;
  };
  EdgeDistance distances[4] = {
      {"left", x},
      {"right", static_cast<float>(Hardware::kScreenWidth) - x},
      {"top", y},
      {"bottom", static_cast<float>(Hardware::kScreenHeight) - y},
  };
  std::sort(std::begin(distances),
            std::end(distances),
            [](const EdgeDistance& a, const EdgeDistance& b) {
              return a.distance > b.distance;
            });
  return distances[random01() < 0.78f ? 0 : 1].edge;
}

void Ant::respawnReplacement(uint32_t nowMs) {
  const Vec2 destination = target_
                               ? Vec2{target_->x, target_->y}
                               : Vec2{(idleMinX_ + idleMaxX_) * 0.5f, idleY_};

  if (target_) {
    const char* edge = farthestSpawnEdge(destination.x, destination.y);
    if (strcmp(edge, "left") == 0) {
      x_ = 5.0f;
      y_ = clampFloat(destination.y + randomRange(-65.0f, 65.0f),
                      12.0f,
                      Hardware::kScreenHeight - 12.0f);
    } else if (strcmp(edge, "right") == 0) {
      x_ = Hardware::kScreenWidth - 5.0f;
      y_ = clampFloat(destination.y + randomRange(-65.0f, 65.0f),
                      12.0f,
                      Hardware::kScreenHeight - 12.0f);
    } else if (strcmp(edge, "top") == 0) {
      x_ = clampFloat(destination.x + randomRange(-85.0f, 85.0f),
                      12.0f,
                      Hardware::kScreenWidth - 12.0f);
      y_ = 5.0f;
    } else {
      x_ = clampFloat(destination.x + randomRange(-85.0f, 85.0f),
                      12.0f,
                      Hardware::kScreenWidth - 12.0f);
      y_ = Hardware::kScreenHeight - 5.0f;
    }
  } else {
    const bool fromLeft = random01() < 0.5f;
    x_ = fromLeft ? idleMinX_ - 7.0f : idleMaxX_ + 7.0f;
    y_ = idleY_ + randomRange(-7.0f, 7.0f);
    idleDirection_ = fromLeft ? 1 : -1;
  }

  const float dx = destination.x - x_;
  const float dy = destination.y - y_;
  const float distance = std::max(0.001f, hypotf(dx, dy));
  const float launchSpeed = target_ ? randomRange(32.0f, 52.0f) : idleSpeed_;
  vx_ = dx / distance * launchSpeed;
  vy_ = dy / distance * launchSpeed;
  phase_ = randomRange(0.0f, kTwoPi);
  wanderPhase_ = randomRange(0.0f, kTwoPi);
  settled_ = false;
  isReplacement_ = true;
  replacementUntil_ = nowMs + kReplacementProtectedMs;
  angle_ = atan2f(vy_, vx_) + kHalfPi;
  lastAngle_ = angle_;
}

void resolveAntOverlaps() {
  const float minimumDistanceSquared = kOverlapResolveDistance * kOverlapResolveDistance;
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    Ant& antA = *gAllAnts[i];
    for (uint8_t j = i + 1; j < gAllAntCount; ++j) {
      Ant& antB = *gAllAnts[j];
      float dx = antB.x_ - antA.x_;
      float dy = antB.y_ - antA.y_;
      float distanceSquared = dx * dx + dy * dy;
      if (distanceSquared >= minimumDistanceSquared) continue;

      if (distanceSquared < 0.0001f) {
        const float randomAngle = randomRange(0.0f, kTwoPi);
        dx = cosf(randomAngle);
        dy = sinf(randomAngle);
        distanceSquared = 1.0f;
      }

      const float distance = sqrtf(distanceSquared);
      const float overlap = (kOverlapResolveDistance - distance) * 0.5f;
      const float nx = dx / distance;
      const float ny = dy / distance;
      float moveA = 0.5f;
      float moveB = 0.5f;
      if (antA.isReplacement_ && !antB.isReplacement_) {
        moveA = 0.12f;
        moveB = 0.88f;
      } else if (antB.isReplacement_ && !antA.isReplacement_) {
        moveA = 0.88f;
        moveB = 0.12f;
      }

      antA.x_ -= nx * overlap * moveA * 2.0f;
      antA.y_ -= ny * overlap * moveA * 2.0f;
      antB.x_ += nx * overlap * moveB * 2.0f;
      antB.y_ += ny * overlap * moveB * 2.0f;
      antA.vx_ -= nx * overlap * 15.0f;
      antA.vy_ -= ny * overlap * 15.0f;
      antB.vx_ += nx * overlap * 15.0f;
      antB.vy_ += ny * overlap * 15.0f;

      if (antA.target_) antA.keepInsideCanvas();
      else antA.keepInsideIdleArea();
      if (antB.target_) antB.keepInsideCanvas();
      else antB.keepInsideIdleArea();
    }
  }
}

Ant* findAntAt(float x, float y, float extraRadius = 0.0f) {
  Ant* bestAnt = nullptr;
  float bestDistanceSquared = INFINITY;
  for (int16_t i = gAllAntCount - 1; i >= 0; --i) {
    Ant* ant = gAllAnts[i];
    const float dx = ant->x() - x;
    const float dy = ant->y() - y;
    const float distanceSquared = dx * dx + dy * dy;
    const float radius = ant->hitRadius(extraRadius);
    if (distanceSquared <= radius * radius && distanceSquared < bestDistanceSquared) {
      bestAnt = ant;
      bestDistanceSquared = distanceSquared;
    }
  }
  return bestAnt;
}

void squashAnt(Ant* ant, uint32_t nowMs) {
  if (!ant) return;
  createBloodSpatter(ant->x(), ant->y(), nowMs);
  ant->respawnReplacement(nowMs);
  ++gSquashedCount;
#if ANT_CLOCK_SERIAL_DEBUG
  Serial.printf("Ant squashed. Total: %lu\n", static_cast<unsigned long>(gSquashedCount));
#endif
}

void scatterAt(float originX, float originY, float radius, float power) {
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    Ant& ant = *gAllAnts[i];
    const float dx = ant.x() - originX;
    const float dy = ant.y() - originY;
    const float distance = hypotf(dx, dy);
    if (distance >= radius) continue;
    const float angle = distance < 0.001f ? randomRange(0.0f, kTwoPi) : atan2f(dy, dx);
    const float impulse = power *
                          (0.35f + (1.0f - distance / radius) * 0.95f) *
                          randomRange(0.82f, 1.18f);
    ant.addImpulse(cosf(angle) * impulse + randomRange(-18.0f, 18.0f),
                   sinf(angle) * impulse + randomRange(-18.0f, 18.0f));
  }
}

void scatterAll() {
  const float centerX = Hardware::kScreenWidth * 0.5f;
  const float centerY = Hardware::kScreenHeight * 0.5f;
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    Ant& ant = *gAllAnts[i];
    float dx = ant.x() - centerX;
    float dy = ant.y() - centerY;
    float distance = hypotf(dx, dy);
    if (distance < 1.0f) {
      const float angle = randomRange(0.0f, kTwoPi);
      dx = cosf(angle);
      dy = sinf(angle);
      distance = 1.0f;
    }
    const float impulse = randomRange(105.0f, 215.0f);
    ant.addImpulse(dx / distance * impulse + randomRange(-68.0f, 68.0f),
                   dy / distance * impulse + randomRange(-68.0f, 68.0f));
  }
}

bool readTouchPoint(uint16_t& x, uint16_t& y) {
  uint8_t data[Hardware::kTouchMaxPoints * 6 + 2]{};
  const uint16_t readLength = sizeof(data);
  const uint8_t readCommand[11] = {
      0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00,
      static_cast<uint8_t>(readLength >> 8),
      static_cast<uint8_t>(readLength & 0xFF),
      0x00, 0x00, 0x00,
  };

  Wire.beginTransmission(Hardware::kTouchAddress);
  Wire.write(readCommand, sizeof(readCommand));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(Hardware::kTouchAddress, static_cast<uint8_t>(sizeof(data))) != sizeof(data)) {
    return false;
  }
  for (uint8_t& value : data) value = static_cast<uint8_t>(Wire.read());

  if (data[1] == 0 || data[1] > Hardware::kTouchMaxPoints) return false;
  const uint16_t rawX = ((data[2] & 0x0Fu) << 8) | data[3];
  const uint16_t rawY = ((data[4] & 0x0Fu) << 8) | data[5];
  if (rawX > 500 || rawY > 500) return false;

  // Mapping for the logical landscape canvas at rotation 1.
  x = static_cast<uint16_t>(clampInt(static_cast<int16_t>(rawY),
                                     0,
                                     Hardware::kScreenWidth - 1));
  y = static_cast<uint16_t>(clampInt(static_cast<int16_t>(Hardware::kScreenHeight - 1 - rawX),
                                     0,
                                     Hardware::kScreenHeight - 1));
  return true;
}

void updateTouch(uint32_t nowMs) {
  if (nowMs - gLastTouchPollAt < 5) return;
  gLastTouchPollAt = nowMs;

  uint16_t touchX = 0;
  uint16_t touchY = 0;
  const bool pressed = readTouchPoint(touchX, touchY);

  if (pressed) {
    gPointer.x = touchX;
    gPointer.y = touchY;
    if (!gTouchWasPressed) {
      const float extraRadius = kTouchHitRadius - kAntHitRadius;
      Ant* hitAnt = findAntAt(gPointer.x, gPointer.y, extraRadius);
      if (hitAnt) {
        gTouchConsumed = true;
        gPointer.inside = false;
        gPointer.down = false;
        squashAnt(hitAnt, nowMs);
      } else {
        gTouchConsumed = false;
        gPointer.inside = true;
        gPointer.down = true;
        scatterAt(gPointer.x, gPointer.y, 112.0f, 190.0f);
      }
    } else if (!gTouchConsumed) {
      gPointer.inside = true;
      gPointer.down = true;
    }
  } else {
    gPointer.inside = false;
    gPointer.down = false;
    gTouchConsumed = false;
  }

  gTouchWasPressed = pressed;
}

time_t parseBuildTime() {
  char monthText[4]{};
  int day = 1;
  int year = 2026;
  int hour = 12;
  int minute = 0;
  int second = 0;
  sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  constexpr const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* monthLocation = strstr(months, monthText);
  const int month = monthLocation ? static_cast<int>((monthLocation - months) / 3) : 0;
  tm build{};
  build.tm_year = year - 1900;
  build.tm_mon = month;
  build.tm_mday = day;
  build.tm_hour = hour;
  build.tm_min = minute;
  build.tm_sec = second;
  build.tm_isdst = -1;
  return mktime(&build);
}

bool systemTimeIsValid() {
  return time(nullptr) > 1700000000;
}

time_t liveTimeNow(uint32_t nowMs) {
  if (systemTimeIsValid()) return time(nullptr);
  return gFallbackBaseTime + static_cast<time_t>((nowMs - gFallbackBaseMillis) / 1000u);
}

time_t floorToMinute(time_t value) {
  return value - (value % 60);
}

time_t resolveDisplayedTime(uint32_t nowMs) {
  if (gClockMode == ClockMode::Live) return liveTimeNow(nowMs);
  if (gClockMode == ClockMode::Demo) {
    const uint32_t elapsed = nowMs - gLastDemoAdvanceAt;
    if (elapsed >= kDemoIntervalMs) {
      const uint32_t steps = elapsed / kDemoIntervalMs;
      gSimulatedTime += static_cast<time_t>(steps) * 60;
      gLastDemoAdvanceAt += steps * kDemoIntervalMs;
    }
  }
  return gSimulatedTime;
}

void applyDisplayedTime(time_t timestamp, bool force = false) {
  tm local{};
  localtime_r(&timestamp, &local);
  if (!force && local.tm_hour == gShownHour && local.tm_min == gShownMinute) return;

  gShownHour = static_cast<int8_t>(local.tm_hour);
  gShownMinute = static_cast<int8_t>(local.tm_min);
  const uint8_t digits[4] = {
      static_cast<uint8_t>(local.tm_hour / 10),
      static_cast<uint8_t>(local.tm_hour % 10),
      static_cast<uint8_t>(local.tm_min / 10),
      static_cast<uint8_t>(local.tm_min % 10),
  };
  for (uint8_t i = 0; i < kDigitGroupCount; ++i) {
    gDigitGroups[i].setDigit(digits[i], force);
  }
#if ANT_CLOCK_SERIAL_DEBUG
  Serial.printf("Displayed time: %02d:%02d\n", local.tm_hour, local.tm_min);
#endif
}

void setClockMode(ClockMode nextMode, uint32_t nowMs) {
  if (nextMode == gClockMode) return;
  const time_t current = resolveDisplayedTime(nowMs);
  if (nextMode != ClockMode::Live) gSimulatedTime = floorToMinute(current);
  gClockMode = nextMode;
  if (nextMode == ClockMode::Demo) gLastDemoAdvanceAt = nowMs;
  if (nextMode == ClockMode::Live) applyDisplayedTime(liveTimeNow(nowMs), false);
}

void advanceOneMinute(uint32_t nowMs) {
  const time_t current = resolveDisplayedTime(nowMs);
  gSimulatedTime = floorToMinute(current) + 60;
  gClockMode = ClockMode::Manual;
  applyDisplayedTime(gSimulatedTime, false);
}

void updateNetwork(uint32_t nowMs) {
  if (strlen(ANT_CLOCK_WIFI_SSID) == 0) return;

  if (!gWifiStarted ||
      (WiFi.status() != WL_CONNECTED && nowMs - gLastWifiAttemptAt > 20000)) {
    gWifiStarted = true;
    gLastWifiAttemptAt = nowMs;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ANT_CLOCK_WIFI_SSID, ANT_CLOCK_WIFI_PASSWORD);
#if ANT_CLOCK_SERIAL_DEBUG
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", ANT_CLOCK_WIFI_SSID);
#endif
  }

  if (WiFi.status() == WL_CONNECTED && !gNtpConfigured) {
    configTzTime(ANT_CLOCK_TIMEZONE,
                 ANT_CLOCK_NTP_SERVER_1,
                 ANT_CLOCK_NTP_SERVER_2,
                 ANT_CLOCK_NTP_SERVER_3);
    gNtpConfigured = true;
#if ANT_CLOCK_SERIAL_DEBUG
    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("NTP configured; waiting for time sync.");
#endif
  }

  if (gNtpConfigured && systemTimeIsValid() && !gTimeSyncReported) {
    gTimeSyncReported = true;
    applyDisplayedTime(time(nullptr), true);
#if ANT_CLOCK_SERIAL_DEBUG
    tm local{};
    const time_t current = time(nullptr);
    localtime_r(&current, &local);
    char buffer[40]{};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", &local);
    Serial.printf("Time synchronized: %s\n", buffer);
#endif
  }
}

void handleSerialCommand(const String& rawCommand, uint32_t nowMs) {
  String command = rawCommand;
  command.trim();
  command.toLowerCase();
  if (command.length() == 0) return;

  if (command == "l" || command == "live") {
    setClockMode(ClockMode::Live, nowMs);
    Serial.println("Mode: live");
  } else if (command == "n" || command == "next") {
    advanceOneMinute(nowMs);
    Serial.println("Advanced one minute; mode: manual");
  } else if (command == "d" || command == "demo") {
    setClockMode(gClockMode == ClockMode::Demo ? ClockMode::Manual : ClockMode::Demo,
                 nowMs);
    Serial.println(gClockMode == ClockMode::Demo ? "Mode: demo" : "Mode: manual");
  } else if (command == "s" || command == "scatter") {
    scatterAll();
    Serial.println("Scatter");
  } else if (command == "help" || command == "?") {
    Serial.println("Commands: live | next | demo | scatter | time HH:MM | help");
  } else if (command.startsWith("time ")) {
    int hour = -1;
    int minute = -1;
    if (sscanf(command.c_str() + 5, "%d:%d", &hour, &minute) == 2 &&
        hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      const time_t current = resolveDisplayedTime(nowMs);
      tm local{};
      localtime_r(&current, &local);
      local.tm_hour = hour;
      local.tm_min = minute;
      local.tm_sec = 0;
      gSimulatedTime = mktime(&local);
      gClockMode = ClockMode::Manual;
      applyDisplayedTime(gSimulatedTime, true);
      Serial.printf("Manual time set to %02d:%02d\n", hour, minute);
    } else {
      Serial.println("Use: time HH:MM");
    }
  } else {
    Serial.println("Unknown command. Type help.");
  }
}

void updateSerial(uint32_t nowMs) {
  static String command;
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r' || value == '\n') {
      if (command.length() > 0) {
        handleSerialCommand(command, nowMs);
        command = "";
      }
    } else if (value == ' ' && command.length() == 0) {
      scatterAll();
    } else if (command.length() < 40) {
      command += value;
    }
  }
}

void drawAmbientField(uint32_t nowMs) {
  const float pulse = 8.0f + 3.0f * sinf(static_cast<float>(nowMs) * 0.0007f);
  const uint8_t alpha = static_cast<uint8_t>(clampFloat(pulse, 0.0f, 255.0f));
  drawHorizontalLine(49, kAmbientRgb, alpha);
  drawHorizontalLine(Hardware::kScreenHeight - 50, kAmbientRgb, alpha);
}

void renderFrame(float dt, uint32_t nowMs) {
  applyDisplayedTime(resolveDisplayedTime(nowMs), false);
  clearFrame();
  drawAmbientField(nowMs);
  updateAndRenderBloodSpatters(nowMs);

  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    gAllAnts[i]->update(dt, nowMs, gPointer);
  }
  resolveAntOverlaps();
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    if (!gAllAnts[i]->hasTarget()) gAllAnts[i]->render(nowMs);
  }
  for (uint8_t i = 0; i < gAllAntCount; ++i) {
    if (gAllAnts[i]->hasTarget()) gAllAnts[i]->render(nowMs);
  }

  gCanvas.flush();
  ++gFramesSinceReport;
}

bool initializeDisplay() {
  pinMode(Hardware::kBacklightPin, OUTPUT);
  digitalWrite(Hardware::kBacklightPin, LOW);

  if (!gCanvas.begin()) {
    Serial.println("ERROR: display/canvas initialization failed.");
    return false;
  }
  gCanvas.setRotation(Hardware::kCanvasRotation);
  gFramebuffer = gCanvas.getFramebuffer();
  if (!gFramebuffer) {
    Serial.println("ERROR: framebuffer allocation failed.");
    return false;
  }

  clearFrame();
  gCanvas.flush();
  digitalWrite(Hardware::kBacklightPin, HIGH);
  return true;
}

void initializeTouch() {
  Wire.begin(Hardware::kTouchSda, Hardware::kTouchScl);
  Wire.setClock(Hardware::kTouchI2cClock);
  delay(400);
}

void initializeAnts() {
  gAllAntCount = 0;
  for (uint8_t group = 0; group < kDigitGroupCount; ++group) {
    gDigitGroups[group].initialize(group, kDigitX[group]);
    for (uint8_t ant = 0; ant < kMaximumAntsPerDigit; ++ant) {
      gAllAnts[gAllAntCount++] = &gDigitGroups[group].ant(ant);
    }
  }

  for (uint8_t i = 0; i < kColonAntCount; ++i) {
    gColonAnts[i].initialize(4, i, 220.0f, 260.0f, true);
    gColonAnts[i].setTarget(&gColonTargets[i]);
    gAllAnts[gAllAntCount++] = &gColonAnts[i];
  }
}

void reportMemory() {
#if ANT_CLOCK_SERIAL_DEBUG
  Serial.printf("PSRAM size: %u bytes, free: %u bytes\n",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()));
  Serial.printf("Internal free heap: %u bytes\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  Serial.printf("Framebuffer: %u bytes at %p\n",
                static_cast<unsigned>(Hardware::kPanelWidth * Hardware::kPanelHeight * 2),
                static_cast<void*>(gFramebuffer));
#endif
}

void reportFps(uint32_t nowMs) {
#if ANT_CLOCK_SERIAL_DEBUG
  if (nowMs - gLastFpsReportAt >= 5000) {
    const float seconds = static_cast<float>(nowMs - gLastFpsReportAt) / 1000.0f;
    const float fps = seconds > 0.0f ? static_cast<float>(gFramesSinceReport) / seconds : 0.0f;
    Serial.printf("FPS: %.1f | blood: %u | squashed: %lu | Wi-Fi: %s\n",
                  fps,
                  gBloodSpatterCount,
                  static_cast<unsigned long>(gSquashedCount),
                  WiFi.status() == WL_CONNECTED ? "connected" : "offline");
    gLastFpsReportAt = nowMs;
    gFramesSinceReport = 0;
  }
#else
  (void)nowMs;
#endif
}

}  // namespace App

void setup() {
  using namespace App;
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("ESPClock ESP32 starting...");
  Serial.println("Target: JC3248W535C / JC3248W535EN, AXS15231B, 480x320 landscape");

  gRandomState ^= esp_random();
  setenv("TZ", ANT_CLOCK_TIMEZONE, 1);
  tzset();
  gFallbackBaseTime = parseBuildTime();
  gFallbackBaseMillis = millis();
  gSimulatedTime = floorToMinute(gFallbackBaseTime);

  if (!initializeDisplay()) {
    while (true) delay(1000);
  }
  initializeTouch();
  initializeAnts();
  applyDisplayedTime(liveTimeNow(millis()), true);
  renderFrame(1.0f / ANT_CLOCK_TARGET_FPS, millis());
  reportMemory();

  gLastFrameAt = millis();
  gLastDemoAdvanceAt = gLastFrameAt;
  gLastFpsReportAt = gLastFrameAt;

  if (strlen(ANT_CLOCK_WIFI_SSID) == 0) {
    Serial.println("Wi-Fi credentials are empty; using build-time fallback clock.");
    Serial.println("Edit include/user_config.h for NTP-synchronized live time.");
  }
  Serial.println("Serial commands: live, next, demo, scatter, time HH:MM, help");
}

void loop() {
  using namespace App;
  const uint32_t nowMs = millis();
  updateNetwork(nowMs);
  updateTouch(nowMs);
  updateSerial(nowMs);

  const uint32_t elapsed = nowMs - gLastFrameAt;
  if (elapsed >= kFrameIntervalMs) {
    const float dt = clampFloat(static_cast<float>(elapsed) / 1000.0f, 0.001f, 0.05f);
    gLastFrameAt = nowMs;
    renderFrame(dt, nowMs);
  }
  reportFps(nowMs);
  delay(1);
}
