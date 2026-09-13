#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include "ants_v2_renderer.h"
#include "car_clock_renderer.h"
#include "clock_gestures.h"

using Bytes = std::vector<uint8_t>;

Bytes read(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  return Bytes(std::istreambuf_iterator<char>(input), {});
}

void dump(const std::string &path, const uint16_t *pixels) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(pixels), 320 * 480 * 2);
  assert(output.good());
}

int main(int argc, char **argv) {
  using clock_gestures::Action;
  clock_gestures::Gesture gesture;
  gesture.begin(3, 300, 160, 100);
  assert(!gesture.move(3, 220, 165, 300));
  assert(gesture.finish(400) == Action::Next);
  assert(gesture.finish(401) == Action::None);  // One change per release.
  gesture.begin(3, 100, 160, 100);
  gesture.move(3, 180, 155, 300);
  assert(gesture.finish(400) == Action::Previous);
  gesture.begin(3, 100, 160, 100);
  gesture.move(3, 108, 163, 200);
  assert(gesture.finish(250) == Action::Tap);
  gesture.begin(3, 100, 160, 100);
  gesture.move(3, 140, 160, 200);
  assert(gesture.finish(250) == Action::None);  // Short horizontal drag isn't a tap.
  gesture.begin(3, 100, 100, 100);
  assert(gesture.move(3, 180, 150, 300));  // Diagonal/vertical movement is ant drag.
  assert(!gesture.move(3, 185, 155, 350));
  assert(gesture.finish(400) == Action::None);
  gesture.begin(3, 100, 160, 100);
  assert(gesture.move(3, 180, 160, 1100));  // Held drag must not switch modes.
  assert(gesture.finish(1200) == Action::None);
  gesture.begin(3, 100, 160, 100);
  assert(!gesture.move(4, 300, 160, 200));  // Another finger cannot move the gesture.
  gesture.cancel();  // Multi-touch or external mode change cancels the pending tap.
  assert(gesture.finish(300) == Action::None);
  gesture.begin(3, 300, 160, UINT32_MAX - 100);
  gesture.move(3, 220, 160, 50);
  assert(gesture.finish(100) == Action::Next);  // millis() wraparound.
  std::puts("PASS: swipe directions, one-shot release, taps, drags, cancellation and timer wraparound.");
  assert(argc == 2 || (argc == 3 && std::string(argv[2]) == "--preview"));
  const std::string directory = argv[1];
  Bytes gold_data = read(directory + "/gold.bin");
  Bytes idle_data = read(directory + "/idle.bin");
  Bytes fire_data = read(directory + "/fire.bin");
  Bytes legacy_data = read(directory + "/gold_legacy.bin");
  esphome::image::Image gold(gold_data.data(), ants_v2_assets::kAtlasWidths[0], ants_v2_assets::kAtlasHeights[0]);
  esphome::image::Image idle(idle_data.data(), ants_v2_assets::kAtlasWidths[1], ants_v2_assets::kAtlasHeights[1]);
  esphome::image::Image fire(fire_data.data(), ants_v2_assets::kAtlasWidths[2], ants_v2_assets::kAtlasHeights[2]);
  esphome::image::Image legacy(legacy_data.data(), gold.get_width(), gold.get_height(), 24);
  ants_v2::Atlas current, old;
  assert(current.bind(&gold, 0));
  assert(old.bind(&legacy, 0));
  assert(current.layout == ants_v2::PixelLayout::Planar565LE);
  assert(old.layout == ants_v2::PixelLayout::Interleaved565BE);
  for (uint32_t i = 0; i < current.pixels; ++i) {
    uint8_t a, b;
    const auto c = current.read(i, a), d = old.read(i, b);
    assert(a == b && (a == 0 || c == d));
  }
  assert(ants_v2::direction_index(0) == 0);
  assert(ants_v2::direction_index(ants_v2_sim::kHalfPi) == 16);
  assert(ants_v2::direction_index(-ants_v2_sim::kHalfPi) == 48);
  assert(ants_v2::direction_index(ants_v2_sim::kTwoPi) == 0);

  assert(ants_v2_sim::begin());
  auto *original_buffer = ants_v2_sim::gFramebuffer;
  constexpr unsigned guard = 256, count = 320 * 480;
  std::vector<uint16_t> buffer(count + guard * 2, 0xA55A);
  ants_v2_sim::gFramebuffer = buffer.data() + guard;
  ants_v2::gAtlases[0] = current;
  assert(ants_v2::gAtlases[1].bind(&idle, 1));
  assert(ants_v2::gAtlases[2].bind(&fire, 2));
  auto check_guards = [&]() {
    for (unsigned i = 0; i < guard; ++i) {
      assert(buffer[i] == 0xA55A);
      assert(buffer[guard + count + i] == 0xA55A);
    }
  };
  // Off-screen clipping at all four edges, all frame banks, and all directions.
  for (int x : {-80, -1, 0, 1, 240, 479, 480, 560}) {
    for (int y : {-80, -1, 0, 1, 160, 319, 320, 400}) {
      for (uint8_t bank = 0; bank < 3; ++bank) {
        for (uint8_t direction = 0; direction < 64; ++direction) {
          ants_v2::draw_sprite(float(x), float(y), direction * ants_v2_sim::kTwoPi / 64,
                               direction % 12, bank, 182);
        }
      }
      check_guards();
    }
  }

  struct Case { float x, y; uint8_t frame, direction, bank, opacity; };
  const Case cases[] = {
    {100, 100, 0, 0, 0, 255}, {100, 100, 3, 16, 0, 255},
    {100, 100, 6, 32, 0, 255}, {100, 100, 9, 48, 0, 255},
    {100, 100, 5, 7, 1, 255}, {100, 100, 11, 63, 2, 214},
    {0, 0, 4, 13, 0, 255}, {479, 319, 8, 25, 2, 182},
  };
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    ants_v2_sim::clearFrame();
    const auto &c = cases[i];
    ants_v2::draw_sprite(c.x, c.y, c.direction * ants_v2_sim::kTwoPi / 64, c.frame, c.bank, c.opacity);
    dump(directory + "/case_" + std::to_string(i) + ".bin", ants_v2_sim::gFramebuffer);
  }

  esphome::display::Display display;
  ants_v2::set_selected(true);
  clock_router::set_mode(clock_router::DisplayMode::Ants);
  assert(!clock_router::cars_selected());
  for (int frame = 0; frame < 240; ++frame) {
    esphome::host_time_us += 42000;
    ants_v2::render(display, &gold, &idle, &fire);
    if (frame == 70) {
      auto *ant = ants_v2_sim::gAllAnts[0];
      ants_v2_sim::touch_start(ant->x(), ant->y());
      ants_v2_sim::touch_end();
      assert(ants_v2_sim::gSquashedCount > 0);
    }
    if (frame == 110) ants_v2_sim::scatterAll();
    if (frame == 160) ants_v2_sim::advanceOneMinute(esphome::millis());
    if (frame == 220) {
      ants_v2_sim::setClockMode(ants_v2_sim::ClockMode::Manual, esphome::millis());
      // A deterministic full clock for visual review, after allowing it to settle.
      std::tm clock{};
      clock.tm_year = 126; clock.tm_mon = 8; clock.tm_mday = 12;
      clock.tm_hour = 12; clock.tm_min = 34; clock.tm_isdst = -1;
      ants_v2_sim::gSimulatedTime = std::mktime(&clock);
    }
    check_guards();
    assert(ants_v2_sim::gAllAntCount == 70);
    for (unsigned i = 0; i < 70; ++i) {
      const auto &walker = ants_v2::gWalkers[i];
      assert(walker.frame < 12 && walker.cycle >= 0 && walker.cycle < 1);
      assert(std::isfinite(walker.angle));
    }
  }
  for (int frame = 0; frame < 160; ++frame) {
    esphome::host_time_us += 42000;
    ants_v2::render(display, &gold, &idle, &fire);
  }
  dump(directory + "/clock.bin", ants_v2_sim::gFramebuffer);
  assert(ants_v2_sim::gShownHour == 12 && ants_v2_sim::gShownMinute == 34);
  assert(ants_v2_sim::gAllAntCount == 70);
  for (const auto &digit : ants_v2_sim::DigitLayouts::kDigits) {
    assert(digit.count <= 17);
    for (unsigned i = 0; i < digit.count; ++i)
      for (unsigned j = i + 1; j < digit.count; ++j) {
        float dx = (digit.points[i].x - digit.points[j].x) * 60;
        float dy = (digit.points[i].y - digit.points[j].y) * 132;
        assert(dx*dx + dy*dy >= 19.0f*19.0f);
      }
  }
  // Optional documentation capture uses the real renderer and touch simulation.
  // Raw frames stay in the external build directory.
  if (argc == 3) {
    for (int frame = 0; frame < 400; ++frame) {
      esphome::host_time_us += 42000;
      if (frame == 24) ants_v2_sim::advanceOneMinute(esphome::millis());
      if (frame == 120) {
        auto *ant = ants_v2_sim::gAllAnts[0];
        ants_v2_sim::touch_start(ant->x(), ant->y());
        ants_v2_sim::touch_end();
        assert(ants_v2_sim::gSquashedCount > 0);
      }
      if (frame == 190) ants_v2_sim::touch_start(160, 80);
      if (frame > 190 && frame < 230)
        ants_v2_sim::touch_move(160, 80 + (frame - 190) * 4);
      if (frame == 230) ants_v2_sim::touch_end();
      ants_v2::render(display, &gold, &idle, &fire);
      check_guards();
      if (frame % 2 == 0)
        dump(directory + "/preview_" + std::to_string(frame / 2) + ".bin",
             ants_v2_sim::gFramebuffer);
    }
  }
  // Stationary ants smoothly finish a stride and hold the resting frame.
  const auto *original_ant = ants_v2_sim::gAllAnts[0];
  ants_v2_sim::Ant stationary;
  ants_v2::gWalkers[0].replacement_until = stationary.replacementUntil();
  ants_v2::gWalkers[0].cycle = 0.45f;
  ants_v2_sim::gAllAnts[0] = &stationary;
  ants_v2::update_walker(0, 0.042f, 0.2f, false);
  assert(ants_v2::gWalkers[0].cycle > 0.45f && ants_v2::gWalkers[0].frame != 0);
  for (int i = 0; i < 30; ++i) ants_v2::update_walker(0, 0.042f, 0.2f, false);
  assert(ants_v2::gWalkers[0].frame == 0);
  stationary.changeVelocity(30.0f, 0.0f);
  for (int i = 0; i < 120; ++i) {
    const float previous = ants_v2::gWalkers[0].cycle;
    if (i == 30) stationary.changeVelocity(90.0f, 0.0f);
    ants_v2::update_walker(0, 0.042f, 0.2f, false);
    const float advance = fmodf(ants_v2::gWalkers[0].cycle - previous + 1.0f, 1.0f);
    assert(advance > 0.0f && advance <= 1.9f * 0.042f + 0.00001f);
  }
  ants_v2_sim::gAllAnts[0] = const_cast<ants_v2_sim::Ant *>(original_ant);

  ants_v2::set_selected(false);
  assert(!ants_v2::selected());
  clock_router::set_mode(clock_router::DisplayMode::Cars);
  assert(clock_router::cars_selected());
  ants_v2::set_selected(true);
  clock_router::set_mode(clock_router::DisplayMode::Ants);
  assert(ants_v2_sim::begin());
  assert(ants_v2_sim::gFramebuffer == buffer.data() + guard);
  ants_v2::render(display, &gold, &idle, &fire);
  assert(ants_v2_sim::gAllAntCount == 70);
  check_guards();
  ants_v2_sim::gFramebuffer = original_buffer;
  std::puts("PASS: both image encodings, rotation, clipping guards, 70 ants, touch, scatter, minute changes, settling and mode switching.");
}
