#pragma once
#include <cstdint>
#include <ctime>
#ifdef _WIN32
inline std::tm *localtime_r(const std::time_t *clock, std::tm *result) {
  return localtime_s(result, clock) == 0 ? result : nullptr;
}
#endif
namespace esphome {
inline uint32_t host_time_us = 1000000;
inline uint32_t millis() { return host_time_us / 1000; }
inline uint32_t micros() { return host_time_us; }
}
