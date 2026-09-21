# espClock ESPHome firmware

I use this configuration for the four-mode espClock touchscreen clock:
**Cars**, **Ants**, **Flip Glass**, and **Flip Glass Light**.

See the [main README](../README.md) for animated previews, controls, hardware,
and first-time installation. This configuration requires ESPHome 2026.7.0 or
later, an ESP32-S3 with 16 MB flash and octal PSRAM, and the JC3248W535 display
and touch wiring specified in `espclock.yaml`.

## Files to copy

Put these files in your ESPHome configuration directory, normally
`/config/esphome/` in Home Assistant. Preserve the asset subdirectories:

- `espclock.yaml`
- `car_clock_renderer.h`
- `car_explosion_assets.h`
- `ants_v2_renderer.h`
- `ants_v2_assets.h`
- `ants_v2_simulation.h`
- `clock_gestures.h`
- `flip_glass_renderer.h`
- `flip_glass_assets.h`
- `flip_glass_light_renderer.h`
- `flip_glass_light_assets.h`
- `assets/car.png`
- `assets/car_crushed.png`
- `assets/ants_v2/atlas_gold.png`
- `assets/ants_v2/atlas_idle.png`
- `assets/ants_v2/atlas_fire.png`

Add `wifi_ssid` and `wifi_password` to your existing `secrets.yaml`, or copy
`secrets.example.yaml` to `secrets.yaml` for a new setup. Keep local secrets
private. Both glass faces and the explosion artwork are embedded in the generated
headers, so their source PNGs are only needed for development.

## First installation and updates

1. Open `espclock.yaml` in ESPHome Device Builder and select **Validate**.
2. For the first installation, connect the board over USB and install through
   ESPHome. For a clock already running this configuration on Wi-Fi, select
   **Install → Wirelessly**.
3. Add the discovered ESPHome device to Home Assistant. Its supplied device
   name is **ESPClock** and its hostname is `espclock`.
4. Choose a mode using **Clock Display**, or swipe the touchscreen.

For multiple clocks, give each configuration a unique device name. Home
Assistant supplies the clock time. Set `timezone` under `time:` if you need to
specify a timezone independently of the build environment.

## Modes and features

- **Cars:** seven-segment car digits with arrival and departure animations.
  Taps trigger an explosion, a damaged car driving away, and a replacement.
- **Ants:** 70 ants using PNG artwork with 12 walking frames and 64 directions.
  Taps squash ants; drags scatter them. The current Ants mode was previously
  called `ants_v2`, and those source filenames are retained.
- **Flip Glass:** glass numerals with reflections and a 1.5-second split-flap
  transition for changed digits.
- **Flip Glass Light:** new clear-glass numerals, a pale daylight background,
  and long reflections, with the same 1.5-second transition.
- Swipe left for Cars → Ants → Flip Glass → Flip Glass Light → Cars; swipe right
  to reverse.
- The selected mode is saved across restarts. Cars is the initial default.
- Home Assistant receives the mode select, online status, Wi-Fi signal, and
  speaker media player. Active Bluetooth proxy support is enabled.

`restore_value: true` saves the selection. The `on_value` action applies it on
startup and when changed. A 1 ms preference write interval saves mode changes
without the usual one-minute delay.

## More details

- [Cars touch animation and validation](CARS_TOUCH.md)
- [Ant artwork, interactions, and sprite generation](ANTS_V2.md)
- [Flip Glass renderer and transitions](FLIP_GLASS.md)
- [Flip Glass Light artwork and renderer](FLIP_GLASS_LIGHT.md)
- [Host tests and build commands](../README.md#development-checks)
