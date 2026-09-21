# Pizza clock proposal

Approved visual design reference. The implemented firmware renderer and its
captured previews are documented in `../../../PIZZA.md`.

Open `index.html` through a local HTTP server for the interactive 480 × 320 simulation. It includes all ten digits, individual bite stages, minute changes, hour carry, midnight, pause, replay, and an animation slider.

Each changed digit is eaten in four staggered bites per pepperoni slice. After the old slices disappear, fresh slices drop into the new numeral. Unchanged digits and the colon stay in place. The sequence lasts 3.6 seconds.

`render.js` contains the shared browser and export renderer. `export.cjs` generates the still previews and optional PNG animation frames with `@napi-rs/canvas`. Set `PIZZA_FRAME_DIR` to an external temporary directory for frames. `manifest.json` records source crop rectangles and digit geometry.

Pizza is included in the firmware, Home Assistant selection, and swipe cycle.
This browser simulation remains the original design reference.
