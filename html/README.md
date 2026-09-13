# ESPClock — fire ants and squash interaction

Offline p5.js prototype for the ESP32 clock concept.

## New in this revision

- The two ants forming the `:` are now red-orange **fire ants**.
- Pointer movement still repels and scatters nearby ants.
- Clicking or tapping directly on an ant now:
  1. removes that ant immediately,
  2. leaves a procedural blood spatter at its exact position,
  3. sends a replacement ant in from a distant screen edge to reclaim the same target.
- Blood darkens slightly, then fades after about five seconds.
- Touch hit-testing is intentionally a little more forgiving than mouse hit-testing.

The custom, evenly spaced digit layouts and uniform digit height remain unchanged.

## Files

- `index.html` — fully offline launcher.
- `sketch.js` — animation, interaction, hit-testing, replacement, and blood-spatter logic.
- `assets/ant_walk_0_14x19.png` and `assets/ant_walk_1_14x19.png` — ant reference frames.

## Controls

- **Live time** — computer clock.
- **+1 minute** — test transitions.
- **Auto demo** — advance every three seconds.
- **Scatter** — scatter every ant.
- Mouse/touch movement — repel ants.
- Direct click/tap on an ant — squash it.

Keyboard: `L` live, `N` next minute, `D` demo, `Space` scatter, `F` fullscreen.


## Collision avoidance refinement

This version adds ant-to-ant separation so ants do not visually overlap. Replacement ants that arrive after a squash now have a temporary stronger “clear path” aura, causing nearby ants to move aside and let them pass through without pileups.
