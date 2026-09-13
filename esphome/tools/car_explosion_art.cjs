// Approved large explosion, copied from the 2026-09-13 interactive preview.
// Canvas is injected so this same artwork can be exported without a browser.
const TAU = Math.PI * 2;
    function explosionFrame(createCanvas, frame) {
      const tile = createCanvas(128, 128);
      const g = tile.getContext('2d');
      g.translate(64, 64);
      g.scale(2, 2);
      function disc(x, y, radius, stops) {
        const gradient = g.createRadialGradient(x, y, 0, x, y, radius);
        for (const [at, color] of stops) gradient.addColorStop(at, color);
        g.fillStyle = gradient; g.beginPath(); g.arc(x, y, radius, 0, TAU); g.fill();
      }
      const radii = [9, 16, 20, 22, 23, 25];
      const r = radii[frame];
      if (frame >= 2) {
        const opacity = [0, 0, 0.36, 0.53, 0.32, 0.12][frame];
        for (let i = 0; i < 7; ++i) {
          const a = i * TAU / 7 + 0.27;
          const distance = r * (0.50 + 0.10 * Math.sin(i * 2.3));
          const x = Math.cos(a) * distance, y = Math.sin(a) * distance - (frame - 2) * 1.5;
          disc(x, y, 8 + frame * 0.7 + Math.sin(i * 1.7), [
            [0, `rgba(159,151,136,${opacity})`],
            [0.50, `rgba(102,102,93,${opacity * 0.87})`],
            [1, 'rgba(64,68,61,0)']
          ]);
        }
      }
      if (frame < 4) {
        const strength = [0.8, 1, 0.85, 0.42][frame];
        disc(0, 0, r + 8, [[0, `rgba(255,160,24,${strength * 0.50})`], [1, 'rgba(247,62,2,0)']]);
        for (let i = 0; i < 9; ++i) {
          const a = i * TAU / 9 + 0.15;
          const d = r * (0.48 + 0.08 * Math.sin(i * 2));
          const size = r * (0.37 + 0.05 * Math.cos(i * 3));
          disc(Math.cos(a) * d, Math.sin(a) * d, size, [
            [0, `rgba(255,225,102,${strength})`],
            [0.42, `rgba(255,141,18,${strength})`],
            [0.80, `rgba(237,62,4,${strength * 0.75})`],
            [1, 'rgba(185,34,0,0)']
          ]);
        }
        disc(0, 0, r * 0.62, [
          [0, `rgba(255,255,231,${strength})`],
          [0.42, `rgba(255,239,148,${strength})`],
          [0.80, `rgba(255,188,41,${strength * 0.90})`],
          [1, 'rgba(255,123,0,0)']
        ]);
      }
      if (frame < 5) {
        const opacity = [0.8, 1, 0.9, 0.7, 0.24][frame];
        g.lineCap = 'round';
        for (let i = 0; i < 10; ++i) {
          const a = i * TAU / 10 + 0.13 + 0.08 * Math.sin(i * 1.9);
          const distance = 9 + frame * 4.2 + (i % 3) * 1.5;
          const tail = distance - (frame < 3 ? 4.5 : 2.3);
          g.strokeStyle = `rgba(255,${i % 3 === 0 ? 152 : 215},${i % 3 === 0 ? 25 : 100},${opacity})`;
          g.lineWidth = frame < 3 ? 1.2 : 0.85;
          g.beginPath(); g.moveTo(Math.cos(a) * tail, Math.sin(a) * tail);
          g.lineTo(Math.cos(a) * distance, Math.sin(a) * distance); g.stroke();
        }
      }
      if (frame === 0) disc(0, 0, 8, [[0, 'rgba(255,255,244,1)'], [0.50, 'rgba(255,249,203,0.95)'], [1, 'rgba(255,209,90,0)']]);
      return tile;
    }

module.exports = { explosionFrame };
