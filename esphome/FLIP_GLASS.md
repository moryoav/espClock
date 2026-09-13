# Flip Glass

Clock display selectable through Home Assistant's **Clock Display** or
horizontal swipes: Cars → Ants → Flip Glass → Cars. The selection is
saved across restarts. Ants uses the newer PNG renderer, previously named `ants_v2`.

## Appearance and transition

The ten numerals and colon are extracted from the approved generated design in
`assets/flip_glass/approved-design.png`. Rounded smoky glass, cool edge highlights,
and warm lower highlights are retained. The translucent appearance is baked onto
black in RGB565; this mode has a fixed black background.

At rest the digits are whole. When the time changes, only changed positions flip
for 1.5 seconds, starting together when several positions change. The old upper half
folds toward the horizontal hinge, followed by the new lower half unfolding and
slowing gently into place. The renderer adds a moving highlight, face shading,
and a soft hinge shadow. The temporary split fades out at the end. The colon does
not blink or animate.

A blurred reflection is sampled from the actual animated framebuffer, with a
dark contact gap and a 100-pixel reflection, ending five pixels above the screen
bottom. A brighter linear fade keeps it visible farther down the screen, matching
the approved longer-shadow preview. Static faces need no repeated full-screen
transfers. Mode entry and the first valid Home Assistant time synchronize directly
to the current time, without replaying missed transitions. Until time arrives,
only the colon is shown, rather than presenting an invented time.

The reflection follows the panel's native column order to keep nearby source
pixels in the PSRAM cache. Device logs report completed flip duration, frame count,
and maximum drawing/transfer time for hardware performance checks.
During a flip, only changing digits and their reflection regions are redrawn and
sent to the panel, together with their black upper/lower margins so each transfer
is one contiguous QSPI operation. A one-digit change transfers 21% of a screen.
Tests compare these partial updates and blur margins against full redraws.

## Firmware files

Copy these alongside the existing clock dependencies:

- `espclock.yaml`
- `flip_glass_renderer.h`
- `flip_glass_assets.h`

The generated header contains 281,600 bytes of RGB565 artwork in flash. A 307,200
byte framebuffer is allocated in PSRAM when this mode is first used. No extra
internal-RAM framebuffer or per-frame heap allocations are needed. The PNGs and
previews are development assets; the firmware reads the generated header.

## Actual C++ renderer previews

- [480 × 320 static clock](assets/flip_glass/preview/clock.png)
- [12:45 → 12:46 animation](assets/flip_glass/preview/flip-12-45-to-12-46.gif)
- [23:59 → 00:00 animation](assets/flip_glass/preview/flip-midnight.gif)

These are RGB565 framebuffer captures from the same C++ renderer used in the
device, with pauses before and after each GIF transition for inspection. The
device uses its existing 42 ms display update interval; rendered animation frames
depend on the actual loop cadence.

## Rebuild and validate

```powershell
py -3.14 tools/build_flip_glass_assets.py
py -3.14 tests/verify_flip_glass.py --build-dir C:/tmp/flip-glass-tests
py -3.14 tests/verify_ants_v2.py --build-dir C:/tmp/ants-regression
esphome compile espclock.yaml
```

Asset generation requires Pillow and NumPy. Host validation additionally requires
MSVC on Windows or g++ on Linux. Keep build products outside the source tree.

Tests cover all 1,440 minute boundaries, synchronized changed-digit animation,
unchanged digit and colon pixels, exact settled artwork, midnight, dropped frames,
rapid time corrections, invalid inputs, timer wraparound, clipping guards, mode
re-entry, and idle transfer suppression. All ten glyphs are compared against the
PNG assets in all four slots, independently checking RGB565 packing and physical
display rotation. Visual appearance and physical touch response still need
inspection on the device.
