# ESPClock Desktop Preview

This package gives you a **desktop/browser preview** for the clock project, so you can tune the visuals and animation **without flashing the ESP32 every time**.

## What is included

- `index.html` – the app
- `styles.css` – styling
- `app.js` – the preview logic
- `assets/car.png` – current car sprite
- `assets/car_crushed.png` – transparent damaged car sprite
- `assets/car_explosion.png` – the same six explosion frames used by the device, at both car sizes
- `assets/ant_walk_0.png` and `assets/ant_walk_1.png` – ant frames

## What it can preview

### Cars mode
- current car asset
- seven-segment layout
- straight-line movement
- simple yielding / moving aside behavior
- digit changes as the time changes
- click or tap to squash a digit or colon car with an explosion, sparks and smoke, show its damaged body, drive away, and bring in a fresh car

### Ants mode
- animated ants
- ant-based digit layout
- mouse scatter behavior
- click-to-squish behavior with respawn

## Recommended way to run it

### Windows
Open Command Prompt in this folder and run:

```bat
python -m http.server 8000
```

Then open:

```text
http://localhost:8000
```

### If Python is not installed
You can also use any small local web server.

## Optional helper files
This package includes:
- `run_local_server.bat`
- `run_local_server.sh`

## Notes
- The preview is intentionally focused on **fast iteration**.
- It is not a pixel-perfect ESPHome simulator.
- Use this to iterate on geometry, spacing, animation, and behavior.
- Then move only the approved changes into the ESPHome files.

## Suggested workflow
1. Test layout/animation here.
2. Tell me what to move, e.g.:
   - “move the top segment car 3 px down”
   - “cars should yield more aggressively”
   - “reduce digit width by 4 px”
3. Once it looks right here, we port the exact changes into ESPHome.
