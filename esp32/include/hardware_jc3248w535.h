#pragma once

#include <Arduino.h>

// Hardware profile for the Guition / Jingcai JC3248W535C_I_Y / JC3248W535EN:
// ESP32-S3 N16R8, 3.5-inch 320x480 IPS, AXS15231B QSPI display,
// capacitive touch over I2C. The application uses the panel in landscape,
// exposing a logical 480x320 canvas.
namespace Hardware {
constexpr int16_t kPanelWidth = 320;
constexpr int16_t kPanelHeight = 480;
constexpr int16_t kScreenWidth = 480;
constexpr int16_t kScreenHeight = 320;
constexpr uint8_t kCanvasRotation = 1;

constexpr int8_t kBacklightPin = 1;

// Arduino_ESP32QSPI constructor pins: CS, SCK, D0, D1, D2, D3.
constexpr int8_t kLcdCs = 45;
constexpr int8_t kLcdSck = 47;
constexpr int8_t kLcdD0 = 21;
constexpr int8_t kLcdD1 = 48;
constexpr int8_t kLcdD2 = 40;
constexpr int8_t kLcdD3 = 39;

constexpr uint8_t kTouchAddress = 0x3B;
constexpr int8_t kTouchSda = 4;
constexpr int8_t kTouchScl = 8;
constexpr uint32_t kTouchI2cClock = 400000;
constexpr uint8_t kTouchMaxPoints = 1;

// The touch controller can be polled directly. We intentionally do not depend
// on the disputed TP_INT / TP_RST pin assignments seen on different batches.
constexpr int8_t kTouchInterruptPin = -1;
constexpr int8_t kTouchResetPin = -1;

constexpr int8_t kBootButtonPin = 0;
}  // namespace Hardware
