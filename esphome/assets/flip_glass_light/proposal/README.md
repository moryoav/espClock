# Approved Flip Glass Light design

This artwork and browser simulation were approved on 2026-09-21. The firmware uses the extracted PNGs in this folder. Actual C++ framebuffer previews are in `../preview/`.

Open `index.html` through a local HTTP server. The page supports sample flips, midnight, custom times, and RGB565 display color simulation.

The built-in imagegen tool generated both source images from the supplied light-glass reference. Exact prompts are in `PROMPTS.md`. The original images are preserved in `digits-source.png` and `background-source.png`. `render.js` extracts the connected glyph silhouettes into fixed cells and composes the scene. `export.cjs` exports the digit sheet, clock image, individual glyphs, and manifest using a canvas runtime. The preview is a design simulation, not a capture from compiled firmware.

The browser preview has no package dependencies. Re-running `export.cjs` requires the Node package `@napi-rs/canvas`; an optional `PREVIEW_FONT_PATH` can supply a local TTF font for the digit-sheet labels. Normal firmware builds use the checked-in PNGs and generated header and do not need Node or image generation.

Reviewed outputs: `digits-preview.png` and `clock-12-45.png`. A headless Edge browser loaded the page with no JavaScript errors. Minute and midnight animations settled at the intended time; a custom 08:27 frame was also visually inspected.

The separate **Flip Glass Light** firmware face uses these glyphs and background, long reflections, alpha compositing, and 1.5-second transitions. It is available through Home Assistant's Clock Display selector and horizontal swipes. The original dark Flip Glass face is retained.
