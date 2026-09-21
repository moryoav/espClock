#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#ifdef TEST_FLIP_GLASS_LIGHT
#include "flip_glass_light_renderer.h"
using namespace flip_glass_light;
#else
#include "flip_glass_renderer.h"
using namespace flip_glass;
#endif
constexpr int kPixels = 480 * 320;
void dump(const std::string &path) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(gFramebuffer), kPixels * 2);
  assert(output.good());
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string out = argv[1];
  assert(begin());
  auto *allocated = gFramebuffer;
  constexpr int guard = 128;
  std::vector<uint16_t> memory(kPixels + guard * 2, 0xa55a);
  gFramebuffer = memory.data() + guard;
#ifdef TEST_FLIP_GLASS_LIGHT
  auto *allocated_layer = gLayer;
  std::vector<uint32_t> layer_memory(kWidth * kDigitHeight + guard * 2, 0xa55aa55a);
  gLayer = layer_memory.data() + guard;
#endif
  auto check_bounds = [&]() {
    for (int i = 0; i < guard; ++i) {
      assert(memory[i] == 0xa55a);
      assert(memory[kPixels + guard + i] == 0xa55a);
#ifdef TEST_FLIP_GLASS_LIGHT
      assert(layer_memory[i] == 0xa55aa55a);
      assert(layer_memory[kWidth * kDigitHeight + guard + i] == 0xa55aa55a);
#endif
    }
  };
  // Exercise every minute boundary in a full day, including all carry patterns.
  for (int minute = 0; minute < 1440; ++minute) {
    set_time(minute / 60, minute % 60, 100, true);
    const int old[] = {minute / 600, (minute / 60) % 10, (minute % 60) / 10, minute % 10};
    const int next = (minute + 1) % 1440;
    set_time(next / 60, next % 60, 200);
    const int values[] = {next / 600, (next / 60) % 10, (next % 60) / 10, next % 10};
    for (int i = 0; i < 4; ++i) {
      assert(gDigits[i].flipping == (old[i] != values[i]));
      assert(gDigits[i].value == values[i]);
      if (gDigits[i].flipping) assert(gDigits[i].started == 200);
      gDigits[i].progress(200 + kDurationMs);
      assert(!gDigits[i].flipping);
    }
  }
  assert(kDurationMs == 1500);
  std::puts("PASS: all 1,440 minute boundaries, only changed digits flip, synchronized start and 1500 ms completion.");
  // Incremental drawing and partial panel transfers must exactly match a full
  // reference redraw, for both ordinary changes and simultaneous carry flips.
  for (int minute : {0, 9, 59, 599, 719, 779, 1439}) {
    set_time(minute / 60, minute % 60, 100, true);
    draw_frame(100);
    esphome::display::Display panel;
    transfer(panel, 15, true);
    const int next = (minute + 1) % 1440;
    set_time(next / 60, next % 60, 200);
    for (int frame = 0; frame <= 24; ++frame) {
      uint8_t mask = 0;
      for (int i = 0; i < 4; ++i) if (gDigits[i].flipping) mask |= 1 << i;
      const uint32_t now = 200 + frame * kDurationMs / 24;
      draw_frame(now, mask, false);
      transfer(panel, mask, false);
      const std::vector<uint16_t> incremental(gFramebuffer, gFramebuffer + kPixels);
      assert(std::equal(incremental.begin(), incremental.end(), panel.panel.begin()));
      draw_frame(now);
      assert(std::equal(incremental.begin(), incremental.end(), gFramebuffer));
      check_bounds();
    }
    assert(panel.transferred_pixels < 26 * kPixels);
  }
  std::puts("PASS: partial redraws and panel transfers match full frames exactly, including reflection edges and carry changes.");
  set_time(12, 45, 1000, true);
  draw_frame(1000);
  dump(out + "/clock.bin");
  const std::vector<uint16_t> still(gFramebuffer, gFramebuffer + kPixels);
  set_time(12, 46, 2000);
  draw_frame(2000);
  assert(std::equal(still.begin(), still.end(), gFramebuffer));
  for (int frame = 0; frame <= 24; ++frame) {
    draw_frame(2000 + frame * kDurationMs / 24);
    check_bounds();
    // No animation or reflection disturbance outside the changed last digit.
    for (int x = 0; x < kPositions[3] - 1; ++x)
      for (int y = 0; y < kHeight; ++y)
        assert(gFramebuffer[index(x, y)] == still[index(x, y)]);
    dump(out + "/frame_" + std::to_string(frame) + ".bin");
  }
  const std::vector<uint16_t> settled(gFramebuffer, gFramebuffer + kPixels);
  set_time(12, 46, 4000, true);
  draw_frame(4000);
  assert(std::equal(settled.begin(), settled.end(), gFramebuffer));
  set_time(23, 59, 5000, true);
  set_time(0, 0, 5100);
  for (int frame = 0; frame <= 24; ++frame) {
    draw_frame(5100 + frame * kDurationMs / 24);
    check_bounds();
    dump(out + "/midnight_" + std::to_string(frame) + ".bin");
  }
  // A stalled loop must settle directly, never leave a half-open flap.
  set_time(1, 1, 7000);
  draw_frame(15000);
  for (auto &digit : gDigits) assert(!digit.flipping && digit.value == digit.old_value);
  // A rapid time correction settles on the latest target.
  set_time(3, 59, 15001);
  set_time(4, 0, 15020);
  draw_frame(18000);
  assert(gDigits[0].value == 0 && gDigits[1].value == 4 && gDigits[2].value == 0 && gDigits[3].value == 0);
  set_time(99, -1, 18001);
  assert(gDigits[1].value == 4);
  set_time(12, 45, 0, true);
  set_time(12, 46, UINT32_MAX - 100);
  draw_frame(50);
  assert(gDigits[3].flipping);
  draw_frame(kDurationMs - 101);
  assert(!gDigits[3].flipping);
  check_bounds();
  // Every glyph renders in every slot, including its bottom reflection.
  for (int value = 0; value <= 9; ++value) {
    for (auto &digit : gDigits) digit.set(value, 0, true);
    draw_frame(0);
    check_bounds();
    dump(out + "/glyph_" + std::to_string(value) + ".bin");
  }
  set_selected(true);
  esphome::display::Display display;
  render(display);
  assert(display.transfers == 1);
  render(display);
  assert(display.transfers == 1);  // Static mode does not spam full-frame transfers.
  set_selected(false);
  set_selected(true);
  assert(!gHaveTime);
  render(display);
  assert(display.transfers == 2);
  for (auto &digit : gDigits) assert(!digit.flipping);
  gFramebuffer = allocated;
#ifdef TEST_FLIP_GLASS_LIGHT
  gLayer = allocated_layer;
#endif
  std::puts("PASS: unchanged pixels/colon, exact settled frame, midnight, dropped frames, time corrections, wraparound, bounds, mode re-entry and idle transfer suppression.");
}
