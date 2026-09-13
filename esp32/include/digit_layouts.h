#pragma once

#include <Arduino.h>

namespace DigitLayouts {
struct Point {
  float x;
  float y;
  bool horizontal;
};

struct Definition {
  const Point* points;
  uint8_t count;
};

#define ANT_H(X, Y) {X, Y, true}
#define ANT_V(X, Y) {X, Y, false}

constexpr Point kDigit0[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.12f, 0.25f), ANT_V(0.12f, 0.50f), ANT_V(0.12f, 0.75f),
    ANT_V(0.88f, 0.25f), ANT_V(0.88f, 0.50f), ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit1[] = {
    ANT_V(0.58f, 0.00f), ANT_V(0.58f, 0.25f), ANT_V(0.58f, 0.50f),
    ANT_V(0.58f, 0.75f), ANT_V(0.58f, 1.00f),
};

constexpr Point kDigit2[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.88f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.12f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit3[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.88f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit4[] = {
    ANT_V(0.12f, 0.00f), ANT_V(0.12f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.88f, 0.00f), ANT_V(0.88f, 0.25f), ANT_V(0.88f, 0.50f),
    ANT_V(0.88f, 0.75f), ANT_V(0.88f, 1.00f),
};

constexpr Point kDigit5[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.12f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit6[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.12f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.12f, 0.75f), ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit7[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.88f, 0.25f), ANT_V(0.88f, 0.50f), ANT_V(0.88f, 0.75f),
    ANT_V(0.88f, 1.00f),
};

constexpr Point kDigit8[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.12f, 0.25f), ANT_V(0.88f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.12f, 0.75f), ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

constexpr Point kDigit9[] = {
    ANT_H(0.18f, 0.00f), ANT_H(0.50f, 0.00f), ANT_H(0.82f, 0.00f),
    ANT_V(0.12f, 0.25f), ANT_V(0.88f, 0.25f),
    ANT_H(0.18f, 0.50f), ANT_H(0.50f, 0.50f), ANT_H(0.82f, 0.50f),
    ANT_V(0.88f, 0.75f),
    ANT_H(0.18f, 1.00f), ANT_H(0.50f, 1.00f), ANT_H(0.82f, 1.00f),
};

#undef ANT_H
#undef ANT_V

constexpr Definition kDigits[10] = {
    {kDigit0, uint8_t(sizeof(kDigit0) / sizeof(kDigit0[0]))},
    {kDigit1, uint8_t(sizeof(kDigit1) / sizeof(kDigit1[0]))},
    {kDigit2, uint8_t(sizeof(kDigit2) / sizeof(kDigit2[0]))},
    {kDigit3, uint8_t(sizeof(kDigit3) / sizeof(kDigit3[0]))},
    {kDigit4, uint8_t(sizeof(kDigit4) / sizeof(kDigit4[0]))},
    {kDigit5, uint8_t(sizeof(kDigit5) / sizeof(kDigit5[0]))},
    {kDigit6, uint8_t(sizeof(kDigit6) / sizeof(kDigit6[0]))},
    {kDigit7, uint8_t(sizeof(kDigit7) / sizeof(kDigit7[0]))},
    {kDigit8, uint8_t(sizeof(kDigit8) / sizeof(kDigit8[0]))},
    {kDigit9, uint8_t(sizeof(kDigit9) / sizeof(kDigit9[0]))},
};

constexpr uint8_t kMaximumAntsPerDigit = 13;
}  // namespace DigitLayouts
