#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace clock_gestures {
enum class Action { None, Tap, Next, Previous };

// Delay ant interaction until a tap or drag is known, so swipes cannot squash ants.
struct Gesture {
  bool active = false, dragging = false;
  uint8_t finger = 0;
  int16_t start_x = 0, start_y = 0, x = 0, y = 0;
  int max_dx = 0, max_dy = 0;
  uint32_t started = 0;

  void cancel() { active = dragging = false; }
  void begin(uint8_t id, int16_t px, int16_t py, uint32_t now) {
    active = true;
    dragging = false;
    finger = id;
    start_x = x = px;
    start_y = y = py;
    max_dx = max_dy = 0;
    started = now;
  }
  // True exactly once when a vertical or held drag starts.
  bool move(uint8_t id, int16_t px, int16_t py, uint32_t now) {
    if (!active || id != finger) return false;
    x = px; y = py;
    max_dx = std::max(max_dx, std::abs(int(x) - start_x));
    max_dy = std::max(max_dy, std::abs(int(y) - start_y));
    if (!dragging && (max_dy > 30 || uint32_t(now - started) > 900)) {
      dragging = true;
      return true;
    }
    return false;
  }
  Action finish(uint32_t now) {
    if (!active) return Action::None;
    const bool was_dragging = dragging;
    cancel();
    if (was_dragging) return Action::None;
    const int dx = int(x) - start_x;
    if (uint32_t(now - started) <= 900 && std::abs(dx) >= 60 &&
        std::abs(dx) >= 2 * max_dy)
      return dx < 0 ? Action::Next : Action::Previous;
    return max_dx < 18 && max_dy < 18 ? Action::Tap : Action::None;
  }
};
inline Gesture touch;
}  // namespace clock_gestures
