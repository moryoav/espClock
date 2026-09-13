(() => {
  const WIDTH = 480;
  const HEIGHT = 320;
  const TAU = Math.PI * 2;
  const HALF_PI = Math.PI / 2;

  const DIGIT_MASKS = [
    0b0111111, // 0 A B C D E F
    0b0000110, // 1 B C
    0b1011011, // 2 A B G E D
    0b1001111, // 3 A B C D G
    0b1100110, // 4 F G B C
    0b1101101, // 5 A F G C D
    0b1111101, // 6 A F G E C D
    0b0000111, // 7 A B C
    0b1111111, // 8
    0b1101111  // 9 A B C D F G
  ];

  const SEG = { A:0, B:1, C:2, D:3, E:4, F:5, G:6 };

  const ANT_LAYOUTS = {
    0: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.12,0.25),V(0.12,0.50),V(0.12,0.75),V(0.88,0.25),V(0.88,0.50),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    1: [V(0.58,0.00),V(0.58,0.25),V(0.58,0.50),V(0.58,0.75),V(0.58,1.00)],
    2: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.88,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.12,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    3: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.88,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    4: [V(0.12,0.00),V(0.12,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.88,0.00),V(0.88,0.25),V(0.88,0.50),V(0.88,0.75),V(0.88,1.00)],
    5: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.12,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    6: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.12,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.12,0.75),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    7: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.88,0.25),V(0.88,0.50),V(0.88,0.75),V(0.88,1.00)],
    8: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.12,0.25),V(0.88,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.12,0.75),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)],
    9: [H(0.18,0.00),H(0.50,0.00),H(0.82,0.00),V(0.12,0.25),V(0.88,0.25),H(0.18,0.50),H(0.50,0.50),H(0.82,0.50),V(0.88,0.75),H(0.18,1.00),H(0.50,1.00),H(0.82,1.00)]
  };

  function H(x, y) { return {x, y, angle: HALF_PI}; }
  function V(x, y) { return {x, y, angle: 0}; }

  function clamp(v, a, b) { return Math.max(a, Math.min(b, v)); }
  function lerp(a, b, t) { return a + (b - a) * t; }
  function length(x, y) { return Math.hypot(x, y); }
  function angleLerp(a, b, t) {
    let d = ((b - a + Math.PI) % TAU) - Math.PI;
    if (d < -Math.PI) d += TAU;
    return a + d * t;
  }
  function nearestParallelAngle(current, preferred) {
    const options = [preferred, preferred + Math.PI];
    let best = options[0], bestScore = 1e9;
    for (const candidate of options) {
      let diff = Math.abs((((candidate - current + Math.PI) % TAU) + TAU) % TAU - Math.PI);
      if (diff < bestScore) { bestScore = diff; best = candidate; }
    }
    return best;
  }
  function floorToMinute(date) {
    const d = new Date(date);
    d.setSeconds(0, 0);
    return d;
  }
  function timeDigits(date) {
    const hh = String(date.getHours()).padStart(2, '0');
    const mm = String(date.getMinutes()).padStart(2, '0');
    return [Number(hh[0]), Number(hh[1]), Number(mm[0]), Number(mm[1])];
  }

  class CarsRenderer {
    constructor(canvas, asset, crushedAsset, explosionAsset) {
      this.canvas = canvas;
      this.ctx = canvas.getContext('2d');
      this.asset = asset;
      this.crushedAsset = crushedAsset;
      this.explosionAsset = explosionAsset;
      this.speedMultiplier = 1;
      this.showGuides = true;
      this.cars = [];
      this.colonCars = [];
      this.timeKey = '';
      this.digitX = [30, 124, 296, 390];
      this.digitWidth = 60;
      this.digitY = 112;
      this.scale = 1.05;
      this.baseTimestamp = performance.now();
      this.currentDigits = [-1,-1,-1,-1];
      this.reset();
    }

    reset() {
      this.cars = [];
      this.colonCars = [];
      const sourceW = this.asset.width;
      const sourceH = this.asset.height;
      const carLengthHalf = Math.max(sourceW, sourceH) * this.scale * 0.5;
      const carWidthHalf = Math.min(sourceW, sourceH) * this.scale * 0.5;
      const carHeightHalf = sourceH * this.scale * 0.5;
      const segmentGap = 1;
      const innerGap = 4 + segmentGap;
      const topBottomGap = 4 + segmentGap;
      this.segmentTargets = [];
      for (let slot = 0; slot < 4; slot++) {
        const x = this.digitX[slot];
        const centerX = x + this.digitWidth * 0.5;
        const rightX = x + this.digitWidth;
        const upperY = this.digitY + 8;
        const middleY = upperY + (carLengthHalf + carWidthHalf + innerGap);
        const lowerY = middleY + (carLengthHalf + carWidthHalf + innerGap);
        const topY = upperY - (carLengthHalf + carWidthHalf + topBottomGap);
        const bottomY = lowerY + (carLengthHalf + carWidthHalf + topBottomGap);
        const garageX = [-62, 216, 264, WIDTH + 62][slot];
        const enterFromLeft = slot === 0 || slot === 2;
        const horizAngle = enterFromLeft ? HALF_PI : -HALF_PI;
        const targets = [
          {x:centerX, y:topY + carWidthHalf, angle:horizAngle, garageX, garageY:topY},
          {x:rightX, y:upperY, angle:0, garageX:rightX, garageY:-45},
          {x:rightX, y:lowerY - carHeightHalf, angle:0, garageX:rightX, garageY:HEIGHT+45},
          {x:centerX, y:bottomY - carWidthHalf * 2.5, angle:horizAngle, garageX, garageY:bottomY},
          {x:x, y:lowerY - carHeightHalf, angle:0, garageX:x, garageY:HEIGHT+45},
          {x:x, y:upperY, angle:0, garageX:x, garageY:-45},
          {x:centerX, y:middleY - carWidthHalf, angle:horizAngle, garageX, garageY:middleY}
        ];
        this.segmentTargets.push(targets);
        for (let seg = 0; seg < 7; seg++) {
          this.cars.push(new CarSprite(this.asset, this.crushedAsset, slot, seg, targets[seg], this.scale));
        }
      }
      this.colonCars = [
        new ColonCar(this.asset, this.crushedAsset, 240, 138, 0, 0.56),
        new ColonCar(this.asset, this.crushedAsset, 240, 178, Math.PI, 0.56)
      ];
      this.currentDigits = [-1,-1,-1,-1];
    }

    restartAnimation(date) {
      this.reset();
      this.applyTime(date, true);
    }

    applyTime(date, force = false) {
      const digits = timeDigits(date);
      const now = performance.now();
      digits.forEach((digit, slot) => {
        if (!force && this.currentDigits[slot] === digit) return;
        const nextMask = DIGIT_MASKS[digit];
        const prevMask = this.currentDigits[slot] >= 0 ? DIGIT_MASKS[this.currentDigits[slot]] : 0;
        for (let seg = 0; seg < 7; seg++) {
          const mask = 1 << seg;
          const was = (prevMask & mask) !== 0;
          const becomes = (nextMask & mask) !== 0;
          if (!force && was === becomes) continue;
          const delay = becomes ? (force ? 180 + seg * 95 : 520 + seg * 75) : seg * 55;
          this.cars[slot * 7 + seg].setWanted(becomes, now + delay, force);
        }
        this.currentDigits[slot] = digit;
      });
      this.timeKey = digits.join('');
    }

    update(dt, date) {
      this.applyTime(date, false);
      const now = performance.now();
      for (const car of this.cars) car.update(dt * this.speedMultiplier, now);
      this.resolveYielding(dt * this.speedMultiplier, now);
      for (const colon of this.colonCars) colon.update(dt * this.speedMultiplier, now);
    }

    click(x, y) {
      const now = performance.now();
      let best = null, bestScore = Infinity;
      for (const car of [...this.cars, ...this.colonCars]) {
        const score = car.hitScore(x, y, now);
        if (score >= 0 && score < bestScore) { best = car; bestScore = score; }
      }
      if (best) best.squash(now);
    }

    resolveYielding(dt, now) {
      const visible = this.cars.filter(c => c.visible(now));
      for (let iter = 0; iter < 2; iter++) {
        for (let i = 0; i < visible.length; i++) {
          for (let j = i + 1; j < visible.length; j++) {
            const a = visible[i], b = visible[j];
            if (a.damaged() && b.damaged()) continue;
            const dx = b.x - a.x, dy = b.y - a.y;
            const dist = Math.hypot(dx, dy);
            if (dist < 0.001 || dist > 34) continue;
            const push = (28 - dist) * 0.5;
            if (push <= 0) continue;
            const ux = dx / dist, uy = dy / dist;
            if (a.isMoving(now) && !b.isMoving(now)) {
              b.displace(ux * push * 1.7, uy * push * 1.7);
            } else if (!a.isMoving(now) && b.isMoving(now)) {
              a.displace(-ux * push * 1.7, -uy * push * 1.7);
            } else {
              a.displace(-ux * push * 0.35, -uy * push * 0.35);
              b.displace(ux * push * 0.35, uy * push * 0.35);
            }
          }
        }
      }
    }

    render() {
      const ctx = this.ctx;
      ctx.clearRect(0, 0, WIDTH, HEIGHT);
      this.drawBackground();
      const now = performance.now();
      for (const car of this.cars) car.draw(ctx, now);
      for (const colon of this.colonCars) colon.draw(ctx, now);
      for (const car of [...this.cars, ...this.colonCars]) car.drawExplosion(ctx, now, this.explosionAsset);
    }

    drawBackground() {
      const ctx = this.ctx;
      ctx.fillStyle = '#121719';
      ctx.fillRect(0,0,WIDTH,HEIGHT);
      ctx.fillStyle = '#6f6641';
      for (let x = 7; x < WIDTH; x += 28) {
        ctx.fillRect(x, 26, 10, 1);
        ctx.fillRect(x, HEIGHT - 27, 10, 1);
      }
      ctx.strokeStyle = 'rgba(102,108,100,0.6)';
      ctx.beginPath();
      ctx.moveTo(0, 38); ctx.lineTo(WIDTH, 38);
      ctx.moveTo(0, HEIGHT - 39); ctx.lineTo(WIDTH, HEIGHT - 39);
      ctx.stroke();
      if (this.showGuides) {
        ctx.strokeStyle = 'rgba(90,98,93,0.25)';
        [102,196,284,378].forEach(x => {
          ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, HEIGHT); ctx.stroke();
        });
      }
    }
  }

  class CarSprite {
    constructor(asset, crushedAsset, slot, seg, target, scale) {
      this.asset = asset;
      this.crushedAsset = crushedAsset;
      this.slot = slot;
      this.seg = seg;
      this.target = {...target};
      this.scale = scale;
      this.x = target.garageX;
      this.y = target.garageY;
      this.vx = 0;
      this.vy = 0;
      this.angle = target.angle;
      this.wanted = false;
      this.parked = true;
      this.atTarget = false;
      this.startAt = 0;
      this.crushPhase = 'none';
      this.crushedAt = 0;
      this.tint = ['#ffd119','#ffd119','#ffd119','#ffd119'][slot % 4];
    }
    setWanted(active, startAt, force) {
      this.wanted = active;
      if (this.damaged()) return;
      this.startAt = startAt;
      this.parked = false;
      if (force && active) {
        this.x = this.target.garageX;
        this.y = this.target.garageY;
      }
    }
    damaged() { return this.crushPhase === 'squashing' || this.crushPhase === 'departing'; }
    visible(now) {
      if (this.damaged()) return true;
      if (now < this.startAt) return this.atTarget;
      return !(!this.wanted && this.parked && !this.atTarget);
    }
    isMoving(now) { return this.crushPhase !== 'squashing' && this.visible(now) && now >= this.startAt && !this.parked; }
    displace(dx, dy) {
      if (this.damaged()) return;
      this.x += dx; this.y += dy;
      if (Math.abs(dx) > 0.001 || Math.abs(dy) > 0.001) this.atTarget = false;
    }
    hitScore(x, y, now) {
      if (!this.visible(now) || this.damaged()) return -1;
      const dx = x - this.x, dy = y - this.y;
      const lx = Math.cos(this.angle) * dx + Math.sin(this.angle) * dy;
      const ly = -Math.sin(this.angle) * dx + Math.cos(this.angle) * dy;
      if (Math.abs(lx) > this.asset.width * this.scale / 2 + 4 ||
          Math.abs(ly) > this.asset.height * this.scale / 2 + 4) return -1;
      return dx * dx + dy * dy;
    }
    squash(now) {
      if (!this.visible(now) || this.damaged()) return false;
      this.crushedAt = now;
      this.explosionOrigin = {x: this.x, y: this.y};
      this.crushPhase = 'squashing';
      this.vx = this.vy = 0;
      this.parked = this.atTarget = false;
      const bx = -Math.sin(this.angle), by = Math.cos(this.angle);
      let distance = 10000;
      if (Math.abs(bx) > 0.001) distance = Math.min(distance, ((bx > 0 ? WIDTH + 60 : -60) - this.target.x) / bx);
      if (Math.abs(by) > 0.001) distance = Math.min(distance, ((by > 0 ? HEIGHT + 60 : -60) - this.target.y) / by);
      this.replacementEntry = {x: this.target.x + bx * distance, y: this.target.y + by * distance};
      return true;
    }
    update(dt, now) {
      if (this.crushPhase === 'squashing') {
        if (now - this.crushedAt < 720) return;
        this.crushPhase = 'departing';
      }
      if (this.crushPhase === 'departing') {
        const speed = Math.min(220, 65 + (now - this.crushedAt - 720) * 0.22);
        this.vx = Math.sin(this.angle) * speed;
        this.vy = -Math.cos(this.angle) * speed;
        this.x += this.vx * dt; this.y += this.vy * dt;
        if (this.x >= -60 && this.x <= WIDTH + 60 && this.y >= -60 && this.y <= HEIGHT + 60) return;
        this.vx = this.vy = 0;
        this.startAt = now;
        this.crushPhase = this.wanted ? 'replacing' : 'none';
        this.x = this.wanted ? this.replacementEntry.x : this.target.garageX;
        this.y = this.wanted ? this.replacementEntry.y : this.target.garageY;
        this.parked = !this.wanted;
        return;
      }
      const destX = this.wanted ? this.target.x : this.target.garageX;
      const destY = this.wanted ? this.target.y : this.target.garageY;
      if (now < this.startAt) return;
      const dx = destX - this.x, dy = destY - this.y;
      const dist = Math.hypot(dx, dy);
      if (dist < 1.6) {
        this.x = destX; this.y = destY;
        this.vx = this.vy = 0;
        this.parked = true;
        this.atTarget = this.wanted;
        this.crushPhase = 'none';
        if (this.wanted) this.angle = nearestParallelAngle(this.angle, this.target.angle);
        return;
      }
      this.parked = false;
      this.atTarget = false;
      let speed = Math.min(155, Math.max(24, dist * 3.4));
      if (dist < 26) speed = Math.max(8, dist * 3.2);
      this.vx = dx / Math.max(dist, 0.001) * speed;
      this.vy = dy / Math.max(dist, 0.001) * speed;
      const step = Math.min(dist, speed * dt);
      this.x += dx / dist * step;
      this.y += dy / dist * step;
      const travelAxis = Math.atan2(this.vy, this.vx) + HALF_PI;
      this.angle = nearestParallelAngle(travelAxis, travelAxis);
      if (this.wanted && Math.hypot(destX - this.x, destY - this.y) < 24) {
        this.angle = nearestParallelAngle(this.angle, this.target.angle);
      }
    }
    draw(ctx, now) {
      if (!this.visible(now)) return;
      ctx.save();
      ctx.translate(this.x, this.y);
      ctx.rotate(this.angle);
      const age = now - this.crushedAt;
      const showDamage = this.damaged() && age >= 90;
      const squash = this.crushPhase === 'squashing' && age < 180 ? Math.sin(Math.PI * age / 180) : 0;
      ctx.scale(this.scale * (1 + 0.14 * squash), this.scale * (1 - 0.32 * squash));
      const sprite = showDamage ? this.crushedAsset : this.asset;
      if (this.isColon && !showDamage) ctx.filter = 'hue-rotate(-28deg) saturate(1.5)';
      ctx.drawImage(sprite, -sprite.width/2, -sprite.height/2);
      ctx.restore();
    }
    drawExplosion(ctx, now, atlas) {
      const age = now - this.crushedAt - 90;
      if (!atlas || !this.damaged() || !this.explosionOrigin || age < 0 || age >= 350) return;
      const frame = Math.floor(age * 6 / 350);
      const size = this.isColon ? 72 : 128;
      ctx.drawImage(atlas, frame * 128, this.isColon ? 128 : 0, size, size,
                    this.explosionOrigin.x - size / 2, this.explosionOrigin.y - size / 2, size, size);
    }
  }

  class ColonCar extends CarSprite {
    constructor(asset, crushedAsset, x, y, angle, scale) {
      super(asset, crushedAsset, 0, 0, {x, y, angle, garageX: x, garageY: angle === 0 ? HEIGHT + 45 : -45}, scale);
      this.x = x; this.y = y;
      this.wanted = this.parked = this.atTarget = true;
      this.isColon = true;
    }
  }

  class AntsRenderer {
    constructor(canvas, frames) {
      this.canvas = canvas;
      this.ctx = canvas.getContext('2d');
      this.frames = frames;
      this.speedMultiplier = 1;
      this.showGuides = true;
      this.digitX = [24, 104, 296, 376];
      this.digitY = 82;
      this.digitWidth = 60;
      this.digitHeight = 156;
      this.groups = [];
      this.colonAnts = [];
      this.blood = [];
      this.pointer = { inside:false, x:-9999, y:-9999, down:false };
      this.reset();
    }
    reset() {
      this.groups = this.digitX.map((x, i) => new AntGroup(i, x, this.digitY, this.digitWidth, this.digitHeight, this.frames));
      this.colonAnts = [
        new AntSprite(this.frames, 240, 126, true),
        new AntSprite(this.frames, 240, 194, true)
      ];
      this.colonAnts[0].target = { x:240, y:126, angle:0 };
      this.colonAnts[1].target = { x:240, y:194, angle:Math.PI };
      this.colonAnts.forEach(a => { a.x = a.target.x; a.y = a.target.y; a.angle = a.target.angle; a.settled = true; a.tint = '#d95432'; });
      this.currentDigits = [-1,-1,-1,-1];
    }
    restartAnimation(date) { this.reset(); this.applyTime(date, true); }
    applyTime(date, force=false) {
      const digits = timeDigits(date);
      digits.forEach((d, idx) => {
        if (!force && this.currentDigits[idx] === d) return;
        this.groups[idx].setDigit(d, force);
        this.currentDigits[idx] = d;
      });
    }
    update(dt, date) {
      this.applyTime(date, false);
      for (const group of this.groups) group.update(dt * this.speedMultiplier, this.pointer);
      for (const ant of this.colonAnts) ant.update(dt * this.speedMultiplier, this.pointer, true);
      this.resolveOverlaps(dt * this.speedMultiplier);
      const now = performance.now();
      this.blood = this.blood.filter(b => now - b.t0 < 2500);
    }
    resolveOverlaps(dt) {
      const ants = [...this.groups.flatMap(g => g.ants), ...this.colonAnts].filter(a => !a.dead);
      for (let i = 0; i < ants.length; i++) {
        for (let j = i + 1; j < ants.length; j++) {
          const a = ants[i], b = ants[j];
          const dx = b.x - a.x, dy = b.y - a.y;
          const dist = Math.hypot(dx, dy);
          if (dist < 0.001 || dist > 18) continue;
          const push = (18 - dist) * 0.5;
          const ux = dx / dist, uy = dy / dist;
          a.x -= ux * push * 0.35;
          a.y -= uy * push * 0.35;
          b.x += ux * push * 0.35;
          b.y += uy * push * 0.35;
        }
      }
    }
    render() {
      const ctx = this.ctx;
      ctx.clearRect(0,0,WIDTH,HEIGHT);
      ctx.fillStyle = 'rgb(5,11,13)';
      ctx.fillRect(0,0,WIDTH,HEIGHT);
      if (this.showGuides) this.drawGuides();
      for (const splat of this.blood) drawBlood(ctx, splat.x, splat.y, splat.t0);
      const ants = [...this.groups.flatMap(g => g.ants), ...this.colonAnts];
      ants.forEach(a => a.draw(ctx));
    }
    drawGuides() {
      const ctx = this.ctx;
      ctx.strokeStyle = 'rgba(255,255,255,0.06)';
      [90, 180, 298, 388].forEach(x => { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, HEIGHT); ctx.stroke(); });
    }
    pointerMove(x, y) { this.pointer.inside = true; this.pointer.x = x; this.pointer.y = y; }
    pointerLeave() { this.pointer.inside = false; this.pointer.x = -9999; this.pointer.y = -9999; }
    click(x, y) {
      const ants = [...this.groups.flatMap(g => g.ants), ...this.colonAnts].filter(a => !a.dead && !a.colon);
      let best = null, bestD = 18;
      for (const ant of ants) {
        const d = Math.hypot(ant.x - x, ant.y - y);
        if (d < bestD) { bestD = d; best = ant; }
      }
      if (best) {
        best.squash();
        this.blood.push({x:best.x, y:best.y, t0:performance.now()});
      }
    }
  }

  class AntGroup {
    constructor(slot, x, y, w, h, frames) {
      this.slot = slot;
      this.x = x; this.y = y; this.w = w; this.h = h;
      this.frames = frames;
      this.maxAnts = 13;
      this.ants = Array.from({length:this.maxAnts}, (_, i) => new AntSprite(frames, x + Math.random() * w, y + Math.random() * h, false));
      this.homeSpots = Array.from({length:this.maxAnts}, (_, i) => ({
        x: x + 8 + (i % 3) * 18 + (slot % 2) * 2,
        y: y + h + 18 + Math.floor(i / 3) * 12,
        angle: HALF_PI
      }));
    }
    setDigit(digit, force) {
      const layout = ANT_LAYOUTS[digit];
      for (let i = 0; i < this.ants.length; i++) {
        const ant = this.ants[i];
        ant.dead = false;
        if (i < layout.length) {
          const p = layout[i];
          ant.target = {
            x: this.x + p.x * this.w,
            y: this.y + p.y * this.h,
            angle: p.angle
          };
        } else {
          ant.target = this.homeSpots[i];
        }
        if (force) {
          ant.x = this.homeSpots[i].x; ant.y = this.homeSpots[i].y; ant.angle = ant.target.angle;
        }
      }
    }
    update(dt, pointer) { this.ants.forEach(a => a.update(dt, pointer)); }
  }

  class AntSprite {
    constructor(frames, x, y, colon) {
      this.frames = frames; this.x = x; this.y = y; this.colon = colon;
      this.vx = 0; this.vy = 0;
      this.angle = 0;
      this.target = null;
      this.dead = false;
      this.respawnAt = 0;
      this.tint = '#f1f1ed';
      this.settled = false;
      this.walkOffset = Math.random() * 1000;
    }
    squash() {
      this.dead = true;
      this.respawnAt = performance.now() + 1200;
      this.x = -1000; this.y = -1000;
    }
    update(dt, pointer, fixedColon = false) {
      const now = performance.now();
      if (this.dead) {
        if (now >= this.respawnAt && this.target) {
          this.dead = false;
          this.x = Math.random() < 0.5 ? -25 : WIDTH + 25;
          this.y = clamp(this.target.y + (Math.random() - 0.5) * 40, 10, HEIGHT - 10);
        } else {
          return;
        }
      }
      if (!this.target) return;
      if (fixedColon) {
        this.x = this.target.x;
        this.y = this.target.y;
        this.angle = this.target.angle;
        return;
      }
      let ax = 0, ay = 0;
      const dx = this.target.x - this.x, dy = this.target.y - this.y;
      const dist = Math.hypot(dx, dy);
      if (dist > 0.5) {
        const speed = clamp(dist * 2.3, 24, 118);
        this.vx += (dx / dist * speed - this.vx) * 0.16;
        this.vy += (dy / dist * speed - this.vy) * 0.16;
      }
      if (pointer.inside) {
        const px = this.x - pointer.x, py = this.y - pointer.y;
        const pd = Math.hypot(px, py);
        if (pd < 58 && pd > 0.001) {
          const force = (58 - pd) / 58 * 760;
          ax += px / pd * force * dt;
          ay += py / pd * force * dt;
        }
      }
      this.vx += ax; this.vy += ay;
      this.vx *= 0.94; this.vy *= 0.94;
      this.x += this.vx * dt; this.y += this.vy * dt;
      const moveMag = Math.hypot(this.vx, this.vy);
      if (moveMag > 1.0) {
        this.angle = angleLerp(this.angle, Math.atan2(this.vy, this.vx) + HALF_PI, 0.18);
      } else {
        this.angle = angleLerp(this.angle, this.target.angle, 0.12);
      }
      this.settled = dist < 4 && moveMag < 10;
    }
    draw(ctx) {
      if (this.dead) return;
      const frameIndex = Math.floor((performance.now() + this.walkOffset) / (this.settled ? 540 : 150)) % 2;
      const img = this.frames[frameIndex];
      ctx.save();
      ctx.translate(this.x, this.y);
      ctx.rotate(this.angle);
      ctx.scale(1, 1);
      ctx.drawImage(img, -img.width / 2, -img.height / 2);
      if (this.tint !== '#f1f1ed') {
        ctx.globalCompositeOperation = 'source-atop';
        ctx.fillStyle = this.tint;
        ctx.fillRect(-img.width / 2, -img.height / 2, img.width, img.height);
        ctx.globalCompositeOperation = 'source-over';
      }
      ctx.restore();
    }
  }

  function drawBlood(ctx, x, y, t0) {
    const age = performance.now() - t0;
    const alpha = clamp(1 - age / 2500, 0, 1);
    ctx.save();
    ctx.globalAlpha = alpha * 0.9;
    ctx.fillStyle = '#a2191e';
    for (const [dx,dy,r] of [[0,0,5],[-7,-4,2.8],[8,-2,3],[4,6,2.5],[-5,7,2.2]]) {
      ctx.beginPath(); ctx.arc(x+dx,y+dy,r,0,TAU); ctx.fill();
    }
    ctx.restore();
  }

  async function loadImage(url) {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = url;
    });
  }

  async function init() {
    const canvas = document.getElementById('clockCanvas');
    const statusText = document.getElementById('statusText');
    const modeSelect = document.getElementById('modeSelect');
    const timeInput = document.getElementById('timeInput');
    const pauseBtn = document.getElementById('pauseBtn');
    const nextMinuteBtn = document.getElementById('nextMinuteBtn');
    const restartBtn = document.getElementById('restartBtn');
    const syncNowBtn = document.getElementById('syncNowBtn');
    const speedInput = document.getElementById('speedInput');
    const speedValue = document.getElementById('speedValue');
    const guidesCheckbox = document.getElementById('guidesCheckbox');

    const [carAsset, crushedCarAsset, explosionAsset, ant0, ant1] = await Promise.all([
      loadImage('assets/car.png'),
      loadImage('assets/car_crushed.png'),
      loadImage('assets/car_explosion.png'),
      loadImage('assets/ant_walk_0.png'),
      loadImage('assets/ant_walk_1.png')
    ]);

    const cars = new CarsRenderer(canvas, carAsset, crushedCarAsset, explosionAsset);
    const ants = new AntsRenderer(canvas, [ant0, ant1]);
    let mode = 'cars';
    let running = true;
    let simulatedTime = floorToMinute(new Date());
    let lastFrame = performance.now();
    let accumulatedMinuteMs = 0;

    function refreshTimeInput() {
      const hh = String(simulatedTime.getHours()).padStart(2, '0');
      const mm = String(simulatedTime.getMinutes()).padStart(2, '0');
      timeInput.value = `${hh}:${mm}`;
      statusText.textContent = `Mode: ${mode} · Time: ${hh}:${mm}`;
    }
    refreshTimeInput();
    cars.restartAnimation(simulatedTime);
    ants.restartAnimation(simulatedTime);

    function activeRenderer() { return mode === 'cars' ? cars : ants; }

    modeSelect.addEventListener('change', () => {
      mode = modeSelect.value;
      activeRenderer().restartAnimation(simulatedTime);
      refreshTimeInput();
    });
    timeInput.addEventListener('change', () => {
      const [hh, mm] = timeInput.value.split(':').map(Number);
      simulatedTime.setHours(hh, mm, 0, 0);
      activeRenderer().restartAnimation(simulatedTime);
      refreshTimeInput();
    });
    pauseBtn.addEventListener('click', () => {
      running = !running;
      pauseBtn.textContent = running ? 'Pause' : 'Resume';
    });
    nextMinuteBtn.addEventListener('click', () => {
      simulatedTime = new Date(simulatedTime.getTime() + 60000);
      activeRenderer().restartAnimation(simulatedTime);
      refreshTimeInput();
    });
    syncNowBtn.addEventListener('click', () => {
      simulatedTime = floorToMinute(new Date());
      activeRenderer().restartAnimation(simulatedTime);
      refreshTimeInput();
    });
    restartBtn.addEventListener('click', () => activeRenderer().restartAnimation(simulatedTime));
    speedInput.addEventListener('input', () => {
      const v = Number(speedInput.value);
      speedValue.textContent = `${v.toFixed(2)}×`;
      cars.speedMultiplier = ants.speedMultiplier = v;
    });
    guidesCheckbox.addEventListener('change', () => {
      cars.showGuides = ants.showGuides = guidesCheckbox.checked;
    });

    canvas.addEventListener('mousemove', (ev) => {
      const rect = canvas.getBoundingClientRect();
      const x = (ev.clientX - rect.left) * WIDTH / rect.width;
      const y = (ev.clientY - rect.top) * HEIGHT / rect.height;
      ants.pointerMove(x, y);
    });
    canvas.addEventListener('mouseleave', () => ants.pointerLeave());
    canvas.addEventListener('click', (ev) => {
      const rect = canvas.getBoundingClientRect();
      const x = (ev.clientX - rect.left) * WIDTH / rect.width;
      const y = (ev.clientY - rect.top) * HEIGHT / rect.height;
      activeRenderer().click(x, y);
    });

    function frame(now) {
      const dt = Math.min(0.05, Math.max(0.001, (now - lastFrame) / 1000));
      lastFrame = now;
      if (running) {
        accumulatedMinuteMs += (now ? (dt * 1000) : 0) * cars.speedMultiplier;
        if (accumulatedMinuteMs >= 10000) {
          accumulatedMinuteMs %= 10000;
          simulatedTime = new Date(simulatedTime.getTime() + 60000);
          refreshTimeInput();
        }
        activeRenderer().update(dt, simulatedTime);
      }
      activeRenderer().render();
      requestAnimationFrame(frame);
    }

    statusText.textContent = 'Ready';
    requestAnimationFrame(frame);
  }

  init().catch(err => {
    console.error(err);
    document.getElementById('statusText').textContent = `Failed to load assets: ${err.message}`;
  });
})();
