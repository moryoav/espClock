#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include "car_clock_renderer.h"
using namespace car_clock;

int main(int argc, char **argv) {
  assert(argc == 2);
  constexpr size_t count = 480 * 320, guard = 64;
  std::vector<uint16_t> pixels(count + 2 * guard, 0xBEEF);
  gFramebuffer = pixels.data() + guard;
  const Vec2 positions[] = {{240,160}, {0,0}, {479,319}, {0,319}, {479,0}, {-500,-500}, {980,820}};
  int index = 0;
  for (bool colon : {false, true}) {
    for (uint8_t frame = 0; frame < 6; ++frame) {
      for (const auto &position : positions) {
        clearFrame(kRoadRgb);
        drawExplosion(position.x, position.y, frame, colon);
        std::ofstream output(std::string(argv[1]) + "/explosion-" + std::to_string(index++) + ".bin", std::ios::binary);
        output.write(reinterpret_cast<char *>(gFramebuffer), count * 2);
        for (size_t i = 0; i < guard; ++i)
          assert(pixels[i] == 0xBEEF && pixels[guard + count + i] == 0xBEEF);
      }
    }
  }
  clearFrame(kRoadRgb);
  drawExplosion(240,160,2);
  drawExplosion(267,153,3,true);
  std::ofstream overlap(std::string(argv[1]) + "/overlap.bin", std::ios::binary);
  overlap.write(reinterpret_cast<char *>(gFramebuffer), count * 2);
  const auto original = pixels;
  drawExplosion(240,160,6);  // Invalid frame is a no-op.
  assert(original == pixels);
  gFramebuffer = nullptr;
  drawExplosion(240,160,0);  // Safe before initialization.
  std::puts("PASS: 84 explosion renders, both sizes, clipping on all corners, offscreen bounds, overlap and invalid input.");
}
