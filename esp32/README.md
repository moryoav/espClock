# ESPClock for ESP32-S3 3.5-inch touch display

This is the embedded C++/PlatformIO port of the approved 480×320 browser prototype. It is intended for the **Guition/Jingcai JC3248W535C_I_Y / JC3248W535EN** board: ESP32-S3 N16R8, 16 MB flash, 8 MB OPI PSRAM, 3.5-inch 320×480 IPS display, AXS15231B controller, and capacitive touch.

> **Check the marking on the PCB or enclosure before flashing.** It should identify the board as `JC3248W535...` and the display should be the AXS15231B model. Do not flash this hardware profile onto a different 3.5-inch ESP32 display merely because its resolution is also 320×480.

## What is ported

The screen is used in landscape as a logical **480×320** canvas. The port preserves the browser version's:

- custom, evenly spaced digit layouts and uniform digit height;
- two-frame 14×19 walking-ant silhouettes;
- ivory clock ants and red fire ants for the colon;
- wandering unused ants at the top and bottom;
- organic target seeking, heading, walking animation, settling, and minute changes;
- touch repulsion and scattering;
- tap-to-squash interaction, procedural blood spatter, and replacement ants;
- ant-to-ant collision avoidance and the replacement ant's temporary clear path;
- live time, manual test time, one-minute advance, and accelerated demo mode.

The HTML toolbar and status strip were outside the 480×320 clock canvas, so they are not drawn on the physical display. Equivalent test controls are available over USB serial.

## Project structure

```text
ESPClock/esp32/
├── platformio.ini
├── partitions.csv
├── include/
│   ├── ant_sprite.h
│   ├── digit_layouts.h
│   ├── hardware_jc3248w535.h
│   └── user_config.h
├── src/
│   └── main.cpp
├── tools/
│   ├── build.ps1
│   ├── build-and-flash.ps1
│   └── restore-factory.ps1
├── tests/
│   └── verify_port.py
├── assets/
└── reference/
```

## 1. Configure Wi-Fi and time

Copy `include/user_config.example.h` to `include/user_config.h`, then enter your
Wi-Fi credentials. The local `user_config.h` is excluded from Git:

```cpp
#define ANT_CLOCK_WIFI_SSID "YOUR_WIFI_NAME"
#define ANT_CLOCK_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

The included timezone rule is for Israel/Jerusalem, including daylight-saving changes:

```cpp
#define ANT_CLOCK_TIMEZONE "IST-2IDT,M3.4.4/26,M10.5.0"
```

Leaving the SSID empty is valid. The clock will run immediately from the firmware build time, but it will not be accurately synchronized until Wi-Fi and NTP are available.

## 2. Install the build tools

The easiest route on Windows is **Visual Studio Code + the PlatformIO extension**. PlatformIO Core can also be installed from PowerShell:

```powershell
py -m pip install platformio
```

You already have `esptool`; otherwise:

```powershell
py -m pip install esptool
```

The project pins:

- PlatformIO Espressif32 platform `6.13.0`;
- Arduino-ESP32 core supplied by that platform;
- `GFX Library for Arduino` version `1.6.0`.

The display example used for this board specifically identifies Arduino_GFX 1.6.0 as the compatible version, so do not casually update that dependency during the first hardware test.

## 3. Run the parity checks

From the extracted project directory:

```powershell
py .\tests\verify_port.py
```

This checks that the embedded digit layouts, sprite masks, animation constants, hardware profile, and partition table still match the approved browser prototype.

## 4. Build only

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1
```

Or directly:

```powershell
pio run -e jc3248w535
```

## 5. Build and flash on COM3

Close any serial monitor that currently owns COM3, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-and-flash.ps1 -Port COM3
```

The script builds first and flashes only if the build succeeds. It uses esptool's ROM loader with `--no-stub`, because this particular board stopped long stub-based transfers during the factory-backup operation. It writes:

```text
0x0000   bootloader.bin
0x8000   partitions.bin
0x10000  firmware.bin
```

It does **not** erase the entire 16 MB flash. If the board does not restart automatically after the upload, unplug and reconnect USB once.

## First boot and serial monitor

Open the monitor at 115200 baud:

```powershell
pio device monitor -p COM3 -b 115200
```

Expected startup messages include the target model, PSRAM size, framebuffer allocation, displayed time, Wi-Fi status, and measured frame rate.

Serial commands:

```text
live          return to live time
next          advance one minute and enter manual mode
demo          toggle accelerated one-minute-per-three-seconds mode
scatter       scatter every ant
time 09:59    set a manual test time
help          show the command list
```

## Touch behavior

- Touching or dragging through empty space repels nearby ants.
- Tapping an ant removes it immediately and creates a blood spatter.
- A replacement enters from a distant screen edge and takes the missing target.
- Nearby ants yield to the replacement, and all ants resolve physical overlap.

The touch controller is polled over I²C. The code deliberately does not rely on disputed touch interrupt/reset pin assignments found across board batches.

## Restoring the factory firmware

Keep both verified factory dumps and their matching SHA-256 hashes somewhere outside this project.

To restore a verified 16,777,216-byte full-flash image:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\restore-factory.ps1 `
  -BackupFile .\factory-full-flash-1.bin `
  -Port COM3 `
  -IUnderstandThisOverwritesTheClock
```

The restore script rejects files that are not exactly 16 MB. The eFuse JSON is documentation only; never attempt to flash or “restore” eFuses from it.

## Hardware profile

The current profile is in `include/hardware_jc3248w535.h`:

```text
Display controller: AXS15231B over QSPI
Display pins:        CS 45, SCK 47, D0 21, D1 48, D2 40, D3 39
Backlight:           GPIO 1
Touch controller:    I²C address 0x3B
Touch pins:          SDA 4, SCL 8
Physical panel:      320×480
Logical canvas:      480×320, landscape
Framebuffer:         RGB565, 307,200 bytes
```

## Performance tuning

The default is 24 full frames per second, configured in `include/user_config.h`:

```cpp
#define ANT_CLOCK_TARGET_FPS 24
```

This is a conservative starting point for full-frame QSPI transfers. After confirming stability on the real board, it can be raised incrementally. If the display flickers, tears, resets, or becomes unstable, lower it first.

## Validation status

Completed in the preparation environment:

- JavaScript-to-C++ layout parity checks;
- exact two-frame sprite-mask parity checks;
- animation and interaction constant parity checks;
- uniform-height and partition-layout checks;
- host C++ syntax compilation with Arduino/display interface stubs.

The project could not be compiled with the actual ESP32 PlatformIO toolchain or run on the physical display in the preparation environment. The first target build and hardware run therefore remain the final validation step.
