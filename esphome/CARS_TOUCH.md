# Cars touch interaction

A released tap squashes the nearest car under the finger, including either of
the two smaller colon cars. The body briefly compresses over 180 ms and changes
to the damaged yellow car. It stays in place until 720 ms after the tap, then
accelerates forward offscreen. Once the damaged car has completely left, a fresh
car drives in from the opposite edge and parks in the original segment.

At the impact, 90 ms after the tap, a large explosion plays for 350 ms: a bright
flash, orange fire and sparks, then fading smoke. Its six frames use a 128×128
canvas for digit cars and 72×72 for the smaller colon cars, matching the approved
larger preview. The burst stays centered on the impact and is drawn above all
cars, including neighbors briefly covered by the larger blast.

The damaged car cannot be squashed again. Each car has its own animation state,
so separate taps can damage several cars. Wrecks keep a straight exit path while
normal digit cars move aside. Their parked light beams turn off.
Normal replacement cars retain the existing yielding and parked-light behavior.

Time changes continue during the effect. A segment that is no longer needed
does not get a replacement; a segment that is still needed gets exactly one.
Returning to Cars mode resets the animation and runs the usual clock entry.
Swipes, drags, and cancelled touches do not trigger a car tap.

## Preview

This animation comes from the actual C++ framebuffer at 480×320. It shows a
vertical and a horizontal car being tapped, leaving, and returning.

![Cars tap and replacement](assets/cars/preview/tap-replacement.gif)

The interactive desktop version is in `../pc/index.html`. Run a local HTTP
server in `pc/`, select Cars, and click or tap a car.

## Asset

`assets/car_crushed.png` is a 28×52 RGBA sprite, matching the existing car canvas.
The damaged body is 22×48 pixels. The built-in image tool removed the external
black background from the supplied photo; opaque dark windows are preserved.
The extracted source and preparation prompt are retained in `assets/cars/`.
The YAML compiles only the small sprite, adding 5,824 bytes of RGBA image data.

Rebuild both ESPHome and browser sprites with Pillow:

```powershell
py -3.14 tools/build_car_crushed.py
```

The explosion uses the approved Canvas artwork in `tools/car_explosion_art.cjs`.
With Node.js and `@napi-rs/canvas` 0.1.100 installed (or located via `NODE_PATH`),
rebuild from the local repository's `esphome/` directory:

```powershell
node tools/build_car_explosion.cjs
```

This writes the frame PNGs and manifest in `assets/cars/explosion/`, the shared
PC atlas, and `car_explosion_assets.h`. The original approved preview is retained
beside the PNGs. The firmware data adds 237,052 bytes in flash and stores pixels
in physical framebuffer order. Each hit only selects, clips and blends these
frames; it does not allocate memory or calculate fire, smoke or particle motion.

## Validation

```powershell
py -3.14 tests/verify_cars.py --build-dir C:/tmp/antclock-cars-tests
py -3.14 tests/verify_car_explosion.py --build-dir C:/tmp/antclock-explosion-tests
py -3.14 tests/verify_ants_v2.py --build-dir C:/tmp/antclock-ants-tests
node --check ../pc/app.js
```

The Cars host check covers all 30 tap targets, four headings, the stationary
squash interval, departure and arrival, repeated taps, minute changes during
damage, reset, timer wraparound, gestures, and guarded framebuffer bounds.
The replacement cars must match their original pixels, and all active cars
must return to their original positions. Cars that move aside can face either
direction on their segment, matching the existing parking behavior.
The explosion check compares every frame at both sizes with the exported PNGs,
including all screen corners, fully offscreen positions and overlapping bursts.
Impact timing, expiry, timer wrap and restart cancellation are also covered.
Physical touch response and display performance still require the device.
