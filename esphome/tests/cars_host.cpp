#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "car_clock_renderer.h"
#include "car_sprite_reference.h"
#include "clock_gestures.h"

using namespace car_clock;

static std::vector<uint8_t> load(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}
static void tick(uint32_t now) {
  esphome::host_time_us = now * 1000;
  for (auto &digit : gDigits) digit.update(0.02f, now);
  for (auto &car : gColonCars) car.update(0.02f, now);
  resolveCarYielding(now);
}
static void setTime(int hour, int minute, uint32_t now, bool force = false) {
  const int digits[4] = {hour / 10, hour % 10, minute / 10, minute % 10};
  for (int i = 0; i < 4; ++i) gDigits[i].setDigit(digits[i], now, force);
}
static void frame(const std::string &path, uint32_t now) {
  drawCarParkBackground();
  for (const auto &digit : gDigits) digit.renderLights();
  for (const auto &digit : gDigits) digit.render(now);
  for (const auto &car : gColonCars) car.render(now);
  for (const auto &digit : gDigits) digit.renderExplosions(now);
  for (const auto &car : gColonCars) car.renderExplosion(now);
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<char *>(gFramebuffer), 480 * 320 * 2);
}
static Car parked(float angle, uint32_t now) {
  Car car;
  car.initialize(0, 0, {240, 160, angle, 240, 160});
  car.setWanted(true, now, 0);
  car.update(0.02f, now);
  assert(car.parkedActive());
  return car;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string directory = argv[1];
  auto normalData = load(directory + "/normal.rgba");
  auto damagedData = load(directory + "/damaged.rgba");
  assert(normalData.size() == 28 * 52 * 4 && damagedData.size() == normalData.size());
  esphome::image::Image normal(normalData.data(), 28, 52, 32), damaged(damagedData.data(), 28, 52, 32);
  gCarSprite = &normal; gCrushedCarSprite = &damaged;
  assert(begin());
  // Render directly into a guarded physical framebuffer, including clipped exits.
  std::free(gFramebuffer);
  constexpr size_t pixels = 480 * 320, guard = 64;
  std::vector<uint16_t> guarded(pixels + 2 * guard, 0xBEEF);
  gFramebuffer = guarded.data() + guard;

  // Compare the fast native-row rasterizer against the original floating-point
  // image sampler across rotation, subpixel movement, squash and screen edges.
  uint64_t error = 0, samples = 0;
  int maximumError = 0;
  std::vector<uint16_t> reference(pixels);
  SpriteRasterCache cache;
  SpriteRasterCache unavailableCache;
  unavailableCache.allocationAttempted = true;
  for (const auto *sprite : {&normal, &damaged}) {
    for (float scale : {kDigitCarScale, kColonCarScale}) {
      for (int heading = 0; heading < 24; ++heading) {
        for (const Vec2 position : {Vec2{240.23f, 160.71f}, Vec2{2.13f, 1.37f}, Vec2{477.43f, 319.19f}}) {
          const float angle = heading * kTwoPi / 24;
          const float squash = heading % 3 == 0 ? 1.0f : 0.0f;
          const auto tint = kDigitCarRgb[heading % 4];
          const uint8_t alpha = heading % 2 ? 255 : 173;
          clearFrame(kRoadRgb);
          referenceDrawCarSprite(position.x, position.y, angle, scale, tint, alpha, sprite,
                                 1 + 0.14f * squash, 1 - 0.32f * squash);
          std::copy(gFramebuffer, gFramebuffer + pixels, reference.begin());
          clearFrame(kRoadRgb);
          drawCarSprite(position.x, position.y, angle, scale, tint, alpha, sprite,
                        1 + 0.14f * squash, 1 - 0.32f * squash);
          for (size_t i = 0; i < pixels; ++i) {
            if (reference[i] == rgb565(kRoadRgb) && gFramebuffer[i] == reference[i]) continue;
            const Rgb a = unpack565(reference[i]), b = unpack565(gFramebuffer[i]);
            for (int difference : {abs(int(a.r) - b.r), abs(int(a.g) - b.g), abs(int(a.b) - b.b)}) {
              maximumError = std::max(maximumError, difference);
              error += difference;
              ++samples;
            }
          }
          std::copy(gFramebuffer, gFramebuffer + pixels, reference.begin());
          for (auto *raster : {&cache, &unavailableCache}) {
            // Cache miss, hit and unavailable-PSRAM fallback must match direct
            // rendering exactly, including changing source, tint and scale.
            for (int repeat = 0; repeat < 2; ++repeat) {
              clearFrame(kRoadRgb);
              drawCarSprite(position.x, position.y, angle, scale, tint, alpha, sprite,
                            1 + 0.14f * squash, 1 - 0.32f * squash, raster);
              assert(std::equal(reference.begin(), reference.end(), gFramebuffer));
            }
          }
          // A cached transparent sprite must blend with this frame's lights
          // and road, never with the background present when it was cached.
          clearFrame(kHeadlightRgb);
          drawCarSprite(position.x, position.y, angle, scale, tint, alpha, sprite,
                        1 + 0.14f * squash, 1 - 0.32f * squash);
          std::copy(gFramebuffer, gFramebuffer + pixels, reference.begin());
          clearFrame(kHeadlightRgb);
          drawCarSprite(position.x, position.y, angle, scale, tint, alpha, sprite,
                        1 + 0.14f * squash, 1 - 0.32f * squash, &cache);
          assert(std::equal(reference.begin(), reference.end(), gFramebuffer));
        }
      }
    }
  }
  std::printf("Raster comparison: mean channel error %.3f/255, max %d/255 across %llu samples.\n",
              double(error) / samples, maximumError, static_cast<unsigned long long>(samples));
  assert(double(error) / samples < 0.7 && maximumError <= 17);
  std::free(cache.pixels);
  std::puts("PASS: cached raster pixels, cache invalidation, changing backgrounds and unavailable-PSRAM fallback.");

  // All headings: squash, fixed hold, forward exit, hidden reset, one fresh arrival.
  for (float angle : {0.0f, kHalfPi, kPi, -kHalfPi}) {
    uint32_t now = 1000;
    Car car = parked(angle, now);
    assert(car.explosionFrame(now) == -1);
    assert(car.hitScore(240, 160, now) == 0);
    assert(car.hitScore(240 + cosf(angle) * 30, 160 + sinf(angle) * 30, now) < 0);
    assert(car.squash(now));
    for (uint32_t elapsed = 0; elapsed < 900; ++elapsed) {
      const int expected = elapsed < 90 || elapsed >= 440 ? -1 : int((elapsed - 90) * 6 / 350);
      assert(car.explosionFrame(now + elapsed) == expected);
    }
    assert(!car.squash(now + 1));
    assert(car.hitScore(240, 160, now) < 0);
    for (now = 1020; now < 1720; now += 20) {
      car.update(0.02f, now);
      assert(car.x() == 240 && car.y() == 160);
      car.applyDisplacement(5, 5);
      assert(car.x() == 240 && car.y() == 160);
    }
    car.update(0.02f, now);
    assert(car.crushPhase() == CrushPhase::Departing);
    assert((car.x() - 240) * sinf(angle) - (car.y() - 160) * cosf(angle) > 0);
    bool replacementSeen = false;
    for (; now < 11000; now += 20) {
      const auto previous = car.crushPhase();
      car.update(0.02f, now);
      if (previous == CrushPhase::Departing && car.crushPhase() == CrushPhase::Replacing) {
        assert(car.x() < 0 || car.x() > 480 || car.y() < 0 || car.y() > 320);
        replacementSeen = true;
      }
      clearFrame(kRoadRgb);
      car.render(now);
    }
    assert(replacementSeen && car.parkedActive() && !car.damaged());
    assert(car.x() == 240 && car.y() == 160);
  }

  // Rollover while damaged must not resurrect an obsolete segment.
  Car obsolete = parked(0, 1000);
  assert(obsolete.squash(1000));
  obsolete.setWanted(false, 1250, 200);
  for (uint32_t now = 1020; now < 10000; now += 20) obsolete.update(0.02f, now);
  assert(!obsolete.visible(10000));
  // A segment requested again before departure completes still gets replaced.
  Car restored = parked(0, 1000);
  assert(restored.squash(1000));
  restored.setWanted(false, 1100, 0);
  restored.setWanted(true, 1200, 0);
  for (uint32_t now = 1020; now < 10000; now += 20) restored.update(0.02f, now);
  assert(restored.parkedActive());

  // The crush delay and replacement remain correct across millis() wraparound.
  Car wrapped = parked(kHalfPi, UINT32_MAX - 200);
  assert(wrapped.squash(UINT32_MAX - 200));
  for (uint32_t elapsed = 0; elapsed < 900; ++elapsed) {
    const int expected = elapsed < 90 || elapsed >= 440 ? -1 : int((elapsed - 90) * 6 / 350);
    assert(wrapped.explosionFrame(uint32_t(UINT32_MAX - 200 + elapsed)) == expected);
  }
  for (uint32_t elapsed = 20; elapsed < 10000; elapsed += 20)
    wrapped.update(0.02f, uint32_t(UINT32_MAX - 200 + elapsed));
  assert(wrapped.parkedActive());
  Car delayed;
  delayed.initialize(0, 0, {240, 160, 0, 240, -45});
  delayed.setWanted(true, UINT32_MAX - 100, 200);
  delayed.update(0.02f, UINT32_MAX - 50);
  assert(!delayed.visible(UINT32_MAX - 50));
  delayed.update(0.02f, 101);
  assert(delayed.visible(101) && delayed.y() > -45);

  // Use the actual tap lookup for each of the 28 segments and both colon cars.
  for (int i = 0; i < 4; ++i) {
    gDigits[i].initialize(i, kDigitX[i]);
    gDigits[i].setDigit(8, 1000, true);
  }
  gColonCars[0].initializeColon(240, 138, 0);
  gColonCars[1].initializeColon(240, 178, kPi);
  for (uint32_t now = 1000; now <= 10000; now += 20) tick(now);
  assert(!tap(2, 2) && !tap(-1, 160) && !tap(480, 160));
  for (auto &digit : gDigits) {
    for (uint8_t i = 0; i < 7; ++i) {
      auto &car = digit.car(i);
      assert(car.parkedActive());
      assert(tap(int16_t(lroundf(car.x())), int16_t(lroundf(car.y()))));
      assert(car.damaged());
    }
  }
  for (auto &car : gColonCars) {
    assert(tap(int16_t(lroundf(car.x())), int16_t(lroundf(car.y()))));
    assert(car.damaged());
  }
  clearFrame(kRoadRgb);
  for (const auto &digit : gDigits) digit.renderExplosions(10200);
  for (const auto &car : gColonCars) car.renderExplosion(10200);
  setTime(10, 0, 10100);
  for (uint32_t now = 10020; now <= 25000; now += 20) tick(now);
  const int after[4] = {1, 0, 0, 0};
  for (int d = 0; d < 4; ++d)
    for (uint8_t c = 0; c < 7; ++c)
      assert(gDigits[d].car(c).parkedActive() == bool(SevenSegment::kDigitMasks[after[d]] & (1 << c)));
  for (auto &car : gColonCars) assert(car.parkedActive());

  // Gesture arbitration is unchanged: a swipe, drag, or cancellation is no tap.
  clock_gestures::Gesture gesture;
  gesture.begin(1, 240, 160, 1000);
  gesture.move(1, 150, 160, 1200);
  assert(gesture.finish(1250) == clock_gestures::Action::Next);
  gesture.begin(1, 240, 160, 1000);
  gesture.move(1, 240, 200, 1200);
  assert(gesture.finish(1250) == clock_gestures::Action::None);
  gesture.begin(1, 240, 160, 1000); gesture.cancel();
  assert(gesture.finish(1250) == clock_gestures::Action::None);

  // Produce a complete firmware animation for review (vertical and horizontal).
  for (int i = 0; i < 4; ++i) gDigits[i].initialize(i, kDigitX[i]);
  setTime(12, 48, 30000, true);
  for (uint32_t now = 30000; now <= 40000; now += 20) tick(now);
  struct ParkedPose { bool active; float x, y, angle; } poses[28];
  for (int d = 0; d < 4; ++d) {
    for (uint8_t c = 0; c < 7; ++c) {
      const auto &car = gDigits[d].car(c);
      poses[d * 7 + c] = {car.parkedActive(), car.x(), car.y(), car.angle()};
    }
  }
  frame(directory + "/before.bin", 40000);
  auto &vertical = gDigits[3].car(SevenSegment::B);
  auto &horizontal = gDigits[3].car(SevenSegment::D);
  assert(tap(int16_t(lroundf(vertical.x())), int16_t(lroundf(vertical.y()))));
  assert(tap(int16_t(lroundf(horizontal.x())), int16_t(lroundf(horizontal.y()))));
  for (uint32_t elapsed = 0; elapsed <= 8000; elapsed += 20) {
    tick(40000 + elapsed);
    if (elapsed % 80 == 0) frame(directory + "/frame_" + std::to_string(elapsed / 80) + ".bin", 40000 + elapsed);
  }
  assert(vertical.parkedActive() && horizontal.parkedActive());
  for (int d = 0; d < 4; ++d) {
    for (uint8_t c = 0; c < 7; ++c) {
      const auto &car = gDigits[d].car(c);
      const auto &pose = poses[d * 7 + c];
      assert(car.parkedActive() == pose.active && !car.damaged());
      if (pose.active) {
        assert(hypotf(car.x() - pose.x, car.y() - pose.y) < 0.01f);
        // Existing yielding allows either direction on the same segment axis.
        assert(fabsf(sinf(car.angle() - pose.angle)) < 0.001f);
      }
    }
  }
  // Mode restart discards every pending wreck and starts the normal entry.
  assert(vertical.squash(48000));
  restart_entry_animation();
  for (const auto &digit : gDigits)
    for (uint8_t i = 0; i < 7; ++i) {
      assert(!digit.car(i).damaged());
      assert(digit.car(i).explosionFrame(48150) == -1);
    }
  for (size_t i = 0; i < guard; ++i) {
    assert(guarded[i] == 0xBEEF);
    assert(guarded[guard + pixels + i] == 0xBEEF);
  }
  gFramebuffer = nullptr;
  std::puts("PASS: 30 car tap targets, four headings, crush/hold/exit/replacement, repeat taps, minute changes, restart, timer wrap, gesture arbitration and framebuffer bounds.");
}
