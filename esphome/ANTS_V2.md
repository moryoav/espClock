# Ants

**Ants** is the PNG-based **Clock Display** option previously named `ants_v2`.
It replaces the old Ants screen. The other options are **Cars** and **Flip Glass**.
The source files and asset paths retain their `ants_v2` names.

The v2 simulation in `ants_v2_simulation.h` retains the established movement,
touch, blood and replacement behavior, with independent state and denser layouts.
It allocates one 300 KiB framebuffer directly, without initializing the old renderer.
Digits use up to 17 ants (70 ants total), with 22-pixel vertical steps instead of
39 pixels. Digit height is 132 pixels, centered vertically, instead of 156.
The redundant overlapping target on the middle-right corner of 4 is removed.

## Artwork and animation

- Three source PNGs were created with the built-in image generation tool: body,
  leg, and antenna. Their exact prompts are in `assets/ants_v2/source/prompts.json`.
- Legs are shortened by 14% around their body joints, retaining the same gait.
- An offline two-joint leg rig creates a seamless 12-frame alternating-tripod walk.
  Left-front/right-middle/left-rear move together, opposite the other three legs.
- The same body pixels and body position are used in every frame. Knees articulate
  and antennae sweep gently. Frames are not independently redrawn by AI.
- Walking cadence responds to speed and is capped so that the 24 FPS target can
  show essentially every animation frame. Stationary ants complete their stride
  and hold a neutral pose instead of snapping to a new frame.
- Each frame is pre-rendered into 64 headings, in 5.625-degree steps. Rotations
  are filtered at high resolution before reduction to the small screen size.
- Gold clock ants, smaller translucent wandering ants, and red colon ants each
  have a shared atlas. The colon retains a brightness pulse.

Previews:

- [Walking animation](assets/ants_v2/preview/walking.gif)
- [All twelve walking frames](assets/ants_v2/preview/walking_frames.png)
- [Actual-size pixels](assets/ants_v2/preview/actual_size.png)
- [Clock drawn by the actual C++ renderer in the host test](assets/ants_v2/preview/clock.png)

These previews show artwork and rendering, not measured device performance.

## Rendering cost

The three atlases contain 2,304 variants and compile to 3,548,160 bytes of RGB565
color and 8-bit alpha data. They are shared in flash by all 70 ants, not copied
for each ant. The image files are embedded during ESPHome compilation, like
`assets/car.png`; PNG files are not loaded or decompressed each frame.

ESPHome may warn that the atlas PNGs are large. Their dimensions are intentional:
each packs hundreds of small sprites. Do not add `resize` to these three image
entries; that would invalidate the atlas coordinates.

The new drawing path uses cropped sprites already oriented for the panel's native
memory layout. Each visible pixel needs one source sample. Fully transparent
pixels are skipped, fully opaque pixels are copied, and partially transparent
edges keep their full alpha precision. No runtime sprite rotation, scaling,
bilinear resampling, image allocation, or ordinary-ant color tinting is needed.

Both ESPHome's planar little-endian RGB565/alpha layout and its older interleaved
big-endian layout are supported. Startup probes compare the fast decoder against
ESPHome's public image decoder. An unfamiliar layout falls back to that public
decoder. Atlas dimensions and image types are checked before rendering.

The existing 42 ms display interval is retained, targeting about 24 FPS. Full
screen transfer and the ant simulation still have a cost. Actual smoothness
must be checked on the ESP32; additional animation frames alone do not guarantee
a higher frame rate.

## Rebuild the sprites locally

From this `esphome` directory, with Python 3 and Pillow/NumPy installed:

```powershell
py -3.14 -m pip install -r tools/requirements-ants-v2.txt
py -3.14 tools/build_ants_v2_sprites.py
```

This regenerates the individual walking PNGs, three packed atlas PNGs, their
manifest, `ants_v2_assets.h`, and artwork previews. It uses the saved source
artwork and does not need an image API key. Keep the generated header and all
three atlases together when copying or rebuilding.

For a quick rig preview without packing all rotations:

```powershell
py -3.14 tools/build_ants_v2_sprites.py --preview-only
```

Run the full build command afterwards before using changed artwork in firmware.

## Validation

```powershell
py -3.14 tests/verify_ants_v2.py
esphome config espclock.yaml
esphome compile espclock.yaml
```

The host test needs MSVC on Windows or g++ on Linux. Its executables and binary
fixtures are stored outside the source directory. It checks both image encodings,
all-direction clipping with framebuffer guards, eight pixel-exact reference
renders, fixed body pixels across all frames, and all 2,304 atlas rectangles.
It also executes the real C++ simulation with 70 ants, touch, scattering, minute
changes, resting transitions, and re-entering Ants after selecting Cars.

Host time is simulated for these checks; the reported host FPS is not a benchmark
of the display hardware. A firmware build tests compilation and flash size,
but physical animation and touchscreen behavior still require the device.

## After flashing

Choose **Ants** from **Clock Display**. The selection persists through restarts.
Cars and Flip Glass remain available.

Swipe left to advance Cars → Ants → Flip Glass → Cars; swipe right to reverse.
The selector in Home Assistant updates and the choice is saved across restarts.
A swipe needs at least 60 horizontal pixels in 900 ms, with little vertical
movement. Each release changes one screen, including wraparound at either end.
Taps on ants still squash them, now on release so a swipe does not squash an ant.
Vertical drags, or moving after holding for about a second, retain ant interaction.
Multiple fingers and changes made from Home Assistant cancel a pending gesture.

The `ants_v2` log reports every ten seconds:

```text
FPS ... | interval p95/max ... ms | sim/draw/send ... ms | work max ... ms | over-budget ... | ants 70
```

- `FPS` and `interval p95/max` measure the spacing between frames, including
  scheduling delays. The p95 value is an upper bound from 1 ms histogram buckets;
  values above 255 ms are collected in the last bucket, while `max` is exact.
- `sim/draw/send` separates average movement/animation work, drawing, and the
  synchronous panel transfer.
- `work max` reports the slowest renderer call; `over-budget` counts calls that
  exceeded the 42 ms target. Other ESPHome work can delay frames even when the
  renderer itself stays within its budget.

For useful device feedback, compare settled digits, a minute change, and repeated
touch interactions that move many ants and leave blood effects on screen.
