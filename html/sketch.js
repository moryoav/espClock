/*
 * ESPClock — p5.js prototype
 * Custom digit layouts with evenly spaced ants and uniform overall height.
 * Standard p5 global-mode drawing calls; also runs with bundled p5-lite.
 */

const CONFIG = Object.freeze({
  canvasWidth: 480,
  canvasHeight: 320,
  digitY: 82,
  digitWidth: 60,
  digitHeight: 156,
  digitX: [24, 104, 296, 376],
  targetSpeed: 118,
  idleSpeedMin: 10,
  idleSpeedMax: 21,
  pointerRadius: 58,
  pointerForce: 760,
  demoIntervalMs: 3000,
  background: [5, 11, 13],
  activeAntScale: 1.0,
  idleAntScale: 0.72,
  settledAntScale: 1.0,
  spriteWidth: 14,
  spriteHeight: 19,
  walkFrameMsMoving: 120,
  walkFrameMsIdle: 240,
  walkFrameMsSettled: 540,
  headingSmoothing: 0.18,
  microJitter: 0.12,
  antHitRadiusMouse: 13,
  antHitRadiusTouch: 19,
  bloodLifetimeMs: 5200,
  bloodFadeMs: 1500,
  maximumBloodSpatters: 18,
  separationRadius: 20,
  separationForce: 360,
  respawnClearRadius: 36,
  respawnClearForce: 980,
  overlapResolveDistance: 15.5,
  replacementProtectedMs: 2600
});

const H = (x, y) => ({ x, y, angle: Math.PI / 2 });
const V = (x, y) => ({ x, y, angle: 0 });

const DIGIT_LAYOUTS = Object.freeze({
  0: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.12, 0.25), V(0.12, 0.50), V(0.12, 0.75),
    V(0.88, 0.25), V(0.88, 0.50), V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  1: [
    V(0.58, 0.00), V(0.58, 0.25), V(0.58, 0.50), V(0.58, 0.75), V(0.58, 1.00)
  ],
  2: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.88, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.12, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  3: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.88, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  4: [
    V(0.12, 0.00), V(0.12, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.88, 0.00), V(0.88, 0.25), V(0.88, 0.50), V(0.88, 0.75), V(0.88, 1.00)
  ],
  5: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.12, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  6: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.12, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.12, 0.75), V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  7: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.88, 0.25), V(0.88, 0.50), V(0.88, 0.75), V(0.88, 1.00)
  ],
  8: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.12, 0.25), V(0.88, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.12, 0.75), V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ],
  9: [
    H(0.18, 0.00), H(0.50, 0.00), H(0.82, 0.00),
    V(0.12, 0.25), V(0.88, 0.25),
    H(0.18, 0.50), H(0.50, 0.50), H(0.82, 0.50),
    V(0.88, 0.75),
    H(0.18, 1.00), H(0.50, 1.00), H(0.82, 1.00)
  ]
});

const ANT_FRAME_MASKS = [
  [
    "...#......#...",
    "...###...##...",
    "....######....",
    ".....####.....",
    "##...####.....",
    ".###.####...##",
    "..#######..###",
    "....########..",
    "...###########",
    "..#######...##",
    ".#########....",
    "##.##.#####...",
    "..##.####.##..",
    ".##.######.##.",
    "##..######..##",
    "#...######..##",
    "....######....",
    ".....#####....",
    ".....####....."
  ],
  [
    "...##....##...",
    "...##....##...",
    "....######....",
    ".....####.....",
    ".....####...##",
    "##...####.###.",
    "###..#######..",
    "..########....",
    "###########...",
    "##...#######..",
    "....##########",
    "...#####.##.##",
    "..##.####.##..",
    ".##.######.##.",
    "###.######..##",
    "##..######...#",
    "....######....",
    ".....#####....",
    ".....####....."
  ]
];

const ANT_PIXEL_COORDS = ANT_FRAME_MASKS.map(frame => {
  const coords = [];
  for (let y = 0; y < frame.length; y += 1) {
    for (let x = 0; x < frame[y].length; x += 1) {
      if (frame[y][x] === "#") coords.push([x, y]);
    }
  }
  return coords;
});

let digitGroups = [];
let colonAnts = [];
let allAnts = [];
let clockMode = "live";
let simulatedTime = new Date();
let lastDemoAdvanceAt = 0;
let shownTimeText = "";
let statusUpdatedAt = -Infinity;
let canvasElement = null;
let drawingCtx = null;
let antSpriteCache = null;
let bloodSpatters = [];
let squashedCount = 0;

const pointer = { inside: false, down: false, x: -10000, y: -10000 };

function setup() {
  const renderer = createCanvas(CONFIG.canvasWidth, CONFIG.canvasHeight);
  renderer.parent("canvas-host");
  canvasElement = renderer.elt;
  pixelDensity(1);
  drawingCtx = canvasElement.getContext("2d");
  drawingCtx.imageSmoothingEnabled = false;
  antSpriteCache = buildAntSpriteCache();
  frameRate(60);
  noStroke();

  digitGroups = CONFIG.digitX.map((x, slotIndex) => new DigitGroup(slotIndex, x));
  colonAnts = createColonAnts();
  allAnts = digitGroups.flatMap(group => group.ants).concat(colonAnts);

  installControls();
  installPointerInteraction();
  installKeyboardControls();

  simulatedTime = floorToMinute(new Date());
  lastDemoAdvanceAt = millis();
  applyDisplayedTime(simulatedTime, true);
  updateInterface(true);
}

function draw() {
  const dt = Math.min(0.04, Math.max(0.001, deltaTime / 1000));
  const nowMs = millis();
  applyDisplayedTime(resolveDisplayedTime(nowMs), false);

  background(...CONFIG.background);
  drawAmbientField(nowMs);
  updateAndRenderBloodSpatters(nowMs);

  for (const ant of allAnts) ant.update(dt, nowMs, pointer);
  resolveAntOverlaps(dt);
  for (const ant of allAnts) if (!ant.target) ant.render(nowMs);
  for (const ant of allAnts) if (ant.target) ant.render(nowMs);

  if (nowMs - statusUpdatedAt > 180) {
    updateInterface(false);
    statusUpdatedAt = nowMs;
  }
}

class DigitGroup {
  constructor(slotIndex, x) {
    this.slotIndex = slotIndex;
    this.x = x;
    this.currentDigit = null;
    this.targetCache = new Map();
    for (let digit = 0; digit <= 9; digit += 1) this.targetCache.set(digit, buildDigitTargets(slotIndex, x, digit));

    const maximumTargets = Math.max(...Array.from(this.targetCache.values(), targets => targets.length));
    this.ants = Array.from({ length: maximumTargets }, (_, antIndex) => new Ant({
      groupIndex: slotIndex,
      antIndex,
      idleMinX: Math.max(10, x - 24),
      idleMaxX: Math.min(CONFIG.canvasWidth - 10, x + CONFIG.digitWidth + 24),
      isColon: false
    }));
  }

  setDigit(digit, force) {
    if (!force && digit === this.currentDigit) return;
    this.currentDigit = digit;
    assignTargets(this.ants, this.targetCache.get(digit));
  }
}

class Ant {
  constructor({ groupIndex, antIndex, idleMinX, idleMaxX, isColon }) {
    this.groupIndex = groupIndex;
    this.antIndex = antIndex;
    this.idleMinX = idleMinX;
    this.idleMaxX = idleMaxX;
    this.isColon = isColon;
    this.idleLane = antIndex % 2;
    this.idleY = this.idleLane === 0 ? randomRange(18, 47) : randomRange(CONFIG.canvasHeight - 47, CONFIG.canvasHeight - 18);
    this.x = randomRange(idleMinX, idleMaxX);
    this.y = this.idleY + randomRange(-8, 8);
    this.vx = randomRange(-14, 14);
    this.vy = randomRange(-5, 5);
    this.target = null;
    this.phase = randomRange(0, Math.PI * 2);
    this.wanderPhase = randomRange(0, Math.PI * 2);
    this.idleDirection = Math.random() < 0.5 ? -1 : 1;
    this.idleSpeed = randomRange(CONFIG.idleSpeedMin, CONFIG.idleSpeedMax);
    this.settled = false;
    this.angle = this.idleDirection > 0 ? Math.PI / 2 : -Math.PI / 2;
    this.lastAngle = this.angle;
    this.isReplacement = false;
    this.replacementUntil = 0;
  }

  update(dt, nowMs, activePointer) {
    let ax = 0;
    let ay = 0;

    if (this.target) {
      const t = nowMs * 0.001;
      const microX = Math.sin(t * 1.7 + this.phase) * CONFIG.microJitter;
      const microY = Math.cos(t * 1.3 + this.phase * 1.23) * CONFIG.microJitter;
      const dx = this.target.x + microX - this.x;
      const dy = this.target.y + microY - this.y;
      const distance = Math.hypot(dx, dy);
      this.settled = distance < 2.4 && Math.hypot(this.vx, this.vy) < 6;
      if (distance > 0.0001) {
        const desiredSpeed = Math.min(CONFIG.targetSpeed, distance * 4.2);
        ax += (dx / distance * desiredSpeed - this.vx) * 7.2;
        ay += (dy / distance * desiredSpeed - this.vy) * 7.2;
      }
    } else {
      this.settled = false;
      const wave = Math.sin(nowMs * 0.00075 + this.wanderPhase);
      const desiredVx = this.idleDirection * this.idleSpeed;
      const desiredVy = (this.idleY - this.y) * 1.12 + wave * 4.5;
      ax += (desiredVx - this.vx) * 2.0;
      ay += (desiredVy - this.vy) * 2.0;
      if (this.x < this.idleMinX + 3) this.idleDirection = 1;
      if (this.x > this.idleMaxX - 3) this.idleDirection = -1;
    }

    if (activePointer.inside) {
      const dx = this.x - activePointer.x;
      const dy = this.y - activePointer.y;
      const distance = Math.hypot(dx, dy);
      const radius = activePointer.down ? CONFIG.pointerRadius * 1.28 : CONFIG.pointerRadius;
      if (distance > 0.001 && distance < radius) {
        const falloff = 1 - distance / radius;
        const strength = CONFIG.pointerForce * falloff * falloff * (activePointer.down ? 1.55 : 1);
        ax += dx / distance * strength;
        ay += dy / distance * strength;
      }
    }

    const separation = computeAntSeparation(this, nowMs);
    ax += separation.ax;
    ay += separation.ay;

    this.vx += ax * dt;
    this.vy += ay * dt;
    const speed = Math.hypot(this.vx, this.vy);
    const maximumSpeed = this.target ? 205 : 48;
    if (speed > maximumSpeed) {
      this.vx = this.vx / speed * maximumSpeed;
      this.vy = this.vy / speed * maximumSpeed;
    }

    const drag = Math.pow(this.target ? 0.992 : 0.998, dt * 60);
    this.vx *= drag;
    this.vy *= drag;
    this.x += this.vx * dt;
    this.y += this.vy * dt;
    if (this.target) this.keepInsideCanvas();
    else this.keepInsideIdleArea();
    this.updateHeading();
    if (this.isReplacement) {
      const closeEnough = this.target ? Math.hypot(this.target.x - this.x, this.target.y - this.y) < 20 : true;
      if (nowMs >= this.replacementUntil || closeEnough) this.isReplacement = false;
    }
  }

  updateHeading() {
    let desiredAngle = this.lastAngle;
    const speed = Math.hypot(this.vx, this.vy);
    if (this.target && (this.settled || speed < 3.2)) desiredAngle = this.target.angle ?? this.lastAngle;
    else if (speed > 1.5) desiredAngle = Math.atan2(this.vy, this.vx) + Math.PI / 2;
    else if (!this.target) desiredAngle = (this.idleDirection > 0 ? Math.PI / 2 : -Math.PI / 2) + Math.sin(this.wanderPhase + millis() * 0.001) * 0.1;
    this.angle = lerpAngle(this.lastAngle, desiredAngle, CONFIG.headingSmoothing);
    this.lastAngle = this.angle;
  }

  keepInsideCanvas() {
    const margin = 3;
    if (this.x < margin) { this.x = margin; this.vx = Math.abs(this.vx) * 0.55; }
    else if (this.x > CONFIG.canvasWidth - margin) { this.x = CONFIG.canvasWidth - margin; this.vx = -Math.abs(this.vx) * 0.55; }
    if (this.y < margin) { this.y = margin; this.vy = Math.abs(this.vy) * 0.55; }
    else if (this.y > CONFIG.canvasHeight - margin) { this.y = CONFIG.canvasHeight - margin; this.vy = -Math.abs(this.vy) * 0.55; }
  }

  keepInsideIdleArea() {
    if (this.x < this.idleMinX - 8) {
      this.x = this.idleMinX - 8;
      this.idleDirection = 1;
      this.vx = Math.abs(this.vx);
    } else if (this.x > this.idleMaxX + 8) {
      this.x = this.idleMaxX + 8;
      this.idleDirection = -1;
      this.vx = -Math.abs(this.vx);
    }
    this.y = Math.max(6, Math.min(CONFIG.canvasHeight - 6, this.y));
  }

  render(nowMs) {
    let alpha, scale;
    if (this.target) {
      alpha = this.settled ? 246 : 230;
      scale = this.settled ? CONFIG.settledAntScale : CONFIG.activeAntScale;
      if (this.isColon) {
        const pulse = 0.5 + 0.5 * Math.sin(nowMs * 0.004 + this.phase);
        alpha = 182 + pulse * 64;
        scale += pulse * 0.04;
      }
    } else {
      alpha = 120;
      scale = CONFIG.idleAntScale;
    }

    const speed = Math.hypot(this.vx, this.vy);
    const interval = this.target ? (this.settled ? CONFIG.walkFrameMsSettled : CONFIG.walkFrameMsMoving) : CONFIG.walkFrameMsIdle;
    const frameOffset = Math.floor((this.phase / (Math.PI * 2)) * 2);
    const moving = speed > (this.target ? 3.5 : 1.2);
    const frameIndex = moving ? (Math.floor((nowMs + frameOffset * interval * 0.5) / interval) % 2) : 0;

    drawAntSprite({ x: this.x, y: this.y, angle: this.angle, frameIndex, scale, palette: this.target ? (this.isColon ? "colon" : "active") : "idle", alpha });
  }

  hitRadius(extraRadius = 0) {
    const scale = this.target ? CONFIG.activeAntScale : CONFIG.idleAntScale;
    return Math.max(CONFIG.antHitRadiusMouse, CONFIG.spriteHeight * scale * 0.58) + extraRadius;
  }

  respawnReplacement(nowMs) {
    const destination = this.target
      ? { x: this.target.x, y: this.target.y }
      : { x: (this.idleMinX + this.idleMaxX) / 2, y: this.idleY };

    if (this.target) {
      const edge = farthestSpawnEdge(destination.x, destination.y);
      if (edge === "left") {
        this.x = 5;
        this.y = clamp(destination.y + randomRange(-65, 65), 12, CONFIG.canvasHeight - 12);
      } else if (edge === "right") {
        this.x = CONFIG.canvasWidth - 5;
        this.y = clamp(destination.y + randomRange(-65, 65), 12, CONFIG.canvasHeight - 12);
      } else if (edge === "top") {
        this.x = clamp(destination.x + randomRange(-85, 85), 12, CONFIG.canvasWidth - 12);
        this.y = 5;
      } else {
        this.x = clamp(destination.x + randomRange(-85, 85), 12, CONFIG.canvasWidth - 12);
        this.y = CONFIG.canvasHeight - 5;
      }
    } else {
      const fromLeft = Math.random() < 0.5;
      this.x = fromLeft ? this.idleMinX - 7 : this.idleMaxX + 7;
      this.y = this.idleY + randomRange(-7, 7);
      this.idleDirection = fromLeft ? 1 : -1;
    }

    const dx = destination.x - this.x;
    const dy = destination.y - this.y;
    const distance = Math.max(0.001, Math.hypot(dx, dy));
    const launchSpeed = this.target ? randomRange(32, 52) : this.idleSpeed;
    this.vx = dx / distance * launchSpeed;
    this.vy = dy / distance * launchSpeed;
    this.phase = randomRange(0, Math.PI * 2);
    this.wanderPhase = randomRange(0, Math.PI * 2);
    this.settled = false;
    this.isReplacement = true;
    this.replacementUntil = nowMs + CONFIG.replacementProtectedMs;
    this.angle = Math.atan2(this.vy, this.vx) + Math.PI / 2;
    this.lastAngle = this.angle;
  }

  addImpulse(vx, vy) { this.vx += vx; this.vy += vy; this.settled = false; }
}

function buildAntSpriteCache() {
  return {
    active: ANT_PIXEL_COORDS.map((_, frameIndex) => buildAntSpriteFrame(frameIndex, [240, 220, 151], 0.22)),
    idle: ANT_PIXEL_COORDS.map((_, frameIndex) => buildAntSpriteFrame(frameIndex, [121, 151, 136], 0.12)),
    colon: ANT_PIXEL_COORDS.map((_, frameIndex) => buildAntSpriteFrame(frameIndex, [232, 61, 39], 0.25))
  };
}

function buildAntSpriteFrame(frameIndex, color, shadowAlpha) {
  const padding = 2;
  const sprite = document.createElement("canvas");
  sprite.width = CONFIG.spriteWidth + padding * 2;
  sprite.height = CONFIG.spriteHeight + padding * 2;
  const ctx = sprite.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  const pixels = ANT_PIXEL_COORDS[frameIndex];
  ctx.fillStyle = `rgba(0, 0, 0, ${shadowAlpha})`;
  for (const [px, py] of pixels) ctx.fillRect(padding + px + 1, padding + py + 1, 1, 1);
  ctx.fillStyle = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
  for (const [px, py] of pixels) ctx.fillRect(padding + px, padding + py, 1, 1);
  return sprite;
}

function drawAntSprite({ x, y, angle, frameIndex, scale, palette, alpha }) {
  const ctx = drawingCtx;
  const sprite = antSpriteCache[palette][frameIndex % 2];
  const drawWidth = sprite.width * scale;
  const drawHeight = sprite.height * scale;
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angle);
  ctx.globalAlpha = Math.max(0, Math.min(255, alpha)) / 255;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(sprite, -drawWidth / 2, -drawHeight / 2, drawWidth, drawHeight);
  ctx.restore();
}



function computeAntSeparation(currentAnt, nowMs) {
  let ax = 0;
  let ay = 0;

  for (const otherAnt of allAnts) {
    if (otherAnt === currentAnt) continue;

    const dx = currentAnt.x - otherAnt.x;
    const dy = currentAnt.y - otherAnt.y;
    const distance = Math.hypot(dx, dy);
    if (distance < 0.001) continue;

    const otherIsProtected = otherAnt.isReplacement && nowMs < otherAnt.replacementUntil;
    const currentIsProtected = currentAnt.isReplacement && nowMs < currentAnt.replacementUntil;
    const radius = otherIsProtected || currentIsProtected ? CONFIG.respawnClearRadius : CONFIG.separationRadius;
    if (distance >= radius) continue;

    const falloff = 1 - distance / radius;
    let strength = CONFIG.separationForce * falloff * falloff;

    if (otherIsProtected && !currentIsProtected) strength += CONFIG.respawnClearForce * falloff;
    else if (currentIsProtected && !otherIsProtected) strength *= 0.35;

    ax += dx / distance * strength;
    ay += dy / distance * strength;
  }

  return { ax, ay };
}

function resolveAntOverlaps(dt) {
  const minimumDistance = CONFIG.overlapResolveDistance;
  const minimumDistanceSquared = minimumDistance * minimumDistance;

  for (let i = 0; i < allAnts.length; i += 1) {
    const antA = allAnts[i];
    for (let j = i + 1; j < allAnts.length; j += 1) {
      const antB = allAnts[j];
      let dx = antB.x - antA.x;
      let dy = antB.y - antA.y;
      let distanceSquared = dx * dx + dy * dy;
      if (distanceSquared >= minimumDistanceSquared) continue;

      if (distanceSquared < 0.0001) {
        const angle = randomRange(0, Math.PI * 2);
        dx = Math.cos(angle);
        dy = Math.sin(angle);
        distanceSquared = 1;
      }

      const distance = Math.sqrt(distanceSquared);
      const overlap = (minimumDistance - distance) * 0.5;
      const nx = dx / distance;
      const ny = dy / distance;

      const aProtected = antA.isReplacement;
      const bProtected = antB.isReplacement;
      let moveA = 0.5;
      let moveB = 0.5;
      if (aProtected && !bProtected) { moveA = 0.12; moveB = 0.88; }
      else if (bProtected && !aProtected) { moveA = 0.88; moveB = 0.12; }

      antA.x -= nx * overlap * moveA * 2;
      antA.y -= ny * overlap * moveA * 2;
      antB.x += nx * overlap * moveB * 2;
      antB.y += ny * overlap * moveB * 2;

      antA.vx -= nx * overlap * 15;
      antA.vy -= ny * overlap * 15;
      antB.vx += nx * overlap * 15;
      antB.vy += ny * overlap * 15;

      if (antA.target) antA.keepInsideCanvas(); else antA.keepInsideIdleArea();
      if (antB.target) antB.keepInsideCanvas(); else antB.keepInsideIdleArea();
    }
  }
}
function findAntAt(x, y, extraRadius = 0) {
  let bestAnt = null;
  let bestDistanceSquared = Infinity;

  // Reverse iteration gives visually topmost ants priority when two overlap.
  for (let index = allAnts.length - 1; index >= 0; index -= 1) {
    const ant = allAnts[index];
    const dx = ant.x - x;
    const dy = ant.y - y;
    const distanceSquared = dx * dx + dy * dy;
    const radius = ant.hitRadius(extraRadius);
    if (distanceSquared <= radius * radius && distanceSquared < bestDistanceSquared) {
      bestAnt = ant;
      bestDistanceSquared = distanceSquared;
    }
  }
  return bestAnt;
}

function squashAnt(ant, nowMs) {
  if (!ant) return false;
  createBloodSpatter(ant.x, ant.y, nowMs);
  ant.respawnReplacement(nowMs);
  squashedCount += 1;
  updateInterface(true);
  return true;
}

function farthestSpawnEdge(x, y) {
  const distances = [
    { edge: "left", distance: x },
    { edge: "right", distance: CONFIG.canvasWidth - x },
    { edge: "top", distance: y },
    { edge: "bottom", distance: CONFIG.canvasHeight - y }
  ];
  distances.sort((a, b) => b.distance - a.distance);
  // Usually use the farthest edge, with a small chance of the second-farthest
  // so repeated replacements do not all follow identical paths.
  return distances[Math.random() < 0.78 ? 0 : 1].edge;
}

function createBloodSpatter(x, y, nowMs) {
  const corePoints = [];
  const corePointCount = 16;
  for (let index = 0; index < corePointCount; index += 1) {
    const angle = index / corePointCount * Math.PI * 2;
    const radius = randomRange(4.5, 8.8) * (index % 2 ? 0.76 : 1.0);
    corePoints.push({ angle, radius });
  }

  const lobes = Array.from({ length: 5 }, () => ({
    x: randomRange(-5, 5),
    y: randomRange(-4, 4),
    radiusX: randomRange(2.4, 5.4),
    radiusY: randomRange(1.8, 4.3),
    rotation: randomRange(0, Math.PI)
  }));

  const droplets = Array.from({ length: Math.floor(randomRange(9, 15)) }, () => {
    const angle = randomRange(0, Math.PI * 2);
    const distance = randomRange(8, 24) * Math.pow(Math.random(), 0.62);
    return {
      x: Math.cos(angle) * distance,
      y: Math.sin(angle) * distance,
      radius: randomRange(0.8, 2.2),
      stretch: randomRange(1.0, 2.2),
      rotation: angle + randomRange(-0.35, 0.35)
    };
  });

  bloodSpatters.push({
    x,
    y,
    bornAt: nowMs,
    rotation: randomRange(0, Math.PI * 2),
    corePoints,
    lobes,
    droplets
  });

  if (bloodSpatters.length > CONFIG.maximumBloodSpatters) {
    bloodSpatters.splice(0, bloodSpatters.length - CONFIG.maximumBloodSpatters);
  }
}

function updateAndRenderBloodSpatters(nowMs) {
  bloodSpatters = bloodSpatters.filter(spatter => nowMs - spatter.bornAt < CONFIG.bloodLifetimeMs);
  const ctx = drawingCtx;

  for (const spatter of bloodSpatters) {
    const age = nowMs - spatter.bornAt;
    const appear = Math.min(1, age / 90);
    const fadeStart = CONFIG.bloodLifetimeMs - CONFIG.bloodFadeMs;
    const fade = age <= fadeStart ? 1 : Math.max(0, 1 - (age - fadeStart) / CONFIG.bloodFadeMs);
    const dry = Math.min(1, age / 2200);
    const red = Math.round(205 - dry * 75);
    const green = Math.round(18 - dry * 10);
    const blue = Math.round(28 - dry * 13);

    ctx.save();
    ctx.translate(spatter.x, spatter.y);
    ctx.rotate(spatter.rotation);
    ctx.scale(appear, appear);
    ctx.globalAlpha = fade;

    ctx.fillStyle = `rgb(${red}, ${green}, ${blue})`;
    ctx.beginPath();
    for (let index = 0; index < spatter.corePoints.length; index += 1) {
      const point = spatter.corePoints[index];
      const px = Math.cos(point.angle) * point.radius;
      const py = Math.sin(point.angle) * point.radius;
      if (index === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.closePath();
    ctx.fill();

    for (const lobe of spatter.lobes) {
      ctx.save();
      ctx.translate(lobe.x, lobe.y);
      ctx.rotate(lobe.rotation);
      ctx.beginPath();
      ctx.ellipse(0, 0, lobe.radiusX, lobe.radiusY, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    }

    ctx.fillStyle = `rgb(${Math.min(238, red + 28)}, ${Math.min(35, green + 7)}, ${Math.min(42, blue + 8)})`;
    for (const drop of spatter.droplets) {
      ctx.save();
      ctx.translate(drop.x, drop.y);
      ctx.rotate(drop.rotation);
      ctx.beginPath();
      ctx.ellipse(0, 0, drop.radius * drop.stretch, drop.radius, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    }

    ctx.restore();
  }
}

function buildDigitTargets(slotIndex, x, digit) {
  return DIGIT_LAYOUTS[digit].map((point, pointIndex) => ({
    x: x + point.x * CONFIG.digitWidth,
    y: CONFIG.digitY + point.y * CONFIG.digitHeight,
    key: `${slotIndex}:${digit}:${pointIndex}`,
    angle: point.angle
  }));
}

function createColonAnts() {
  const targets = [
    { x: 240, y: 128, key: "colon:0", angle: Math.PI / 2 },
    { x: 240, y: 192, key: "colon:1", angle: -Math.PI / 2 }
  ];
  return targets.map((target, index) => {
    const ant = new Ant({ groupIndex: 4, antIndex: index, idleMinX: 220, idleMaxX: 260, isColon: true });
    ant.target = target;
    return ant;
  });
}

function assignTargets(ants, targets) {
  const oldByKey = new Map();
  for (const ant of ants) if (ant.target) oldByKey.set(ant.target.key, ant);
  const assigned = new Set();
  const remainingTargets = [];
  for (const target of targets) {
    const existing = oldByKey.get(target.key);
    if (existing && !assigned.has(existing)) { existing.target = target; assigned.add(existing); }
    else remainingTargets.push(target);
  }
  const availableAnts = ants.filter(ant => !assigned.has(ant));
  for (const ant of availableAnts) ant.target = null;

  while (remainingTargets.length && availableAnts.length) {
    let selectedTargetIndex = 0, selectedAntIndex = 0, largestNearestDistance = -1;
    for (let ti = 0; ti < remainingTargets.length; ti += 1) {
      const target = remainingTargets[ti];
      let nearestAntIndex = 0, nearestDistance = Infinity;
      for (let ai = 0; ai < availableAnts.length; ai += 1) {
        const ant = availableAnts[ai];
        const distanceSquared = (ant.x - target.x) ** 2 + (ant.y - target.y) ** 2;
        if (distanceSquared < nearestDistance) { nearestDistance = distanceSquared; nearestAntIndex = ai; }
      }
      if (nearestDistance > largestNearestDistance) { largestNearestDistance = nearestDistance; selectedTargetIndex = ti; selectedAntIndex = nearestAntIndex; }
    }
    const [target] = remainingTargets.splice(selectedTargetIndex, 1);
    const [ant] = availableAnts.splice(selectedAntIndex, 1);
    ant.target = target;
  }
}

function resolveDisplayedTime(nowMs) {
  if (clockMode === "live") return new Date();
  if (clockMode === "demo") {
    const elapsed = nowMs - lastDemoAdvanceAt;
    if (elapsed >= CONFIG.demoIntervalMs) {
      const steps = Math.floor(elapsed / CONFIG.demoIntervalMs);
      simulatedTime = new Date(simulatedTime.getTime() + steps * 60000);
      lastDemoAdvanceAt += steps * CONFIG.demoIntervalMs;
    }
  }
  return simulatedTime;
}

function applyDisplayedTime(date, force) {
  const hours = date.getHours(), minutes = date.getMinutes();
  const digits = [Math.floor(hours / 10), hours % 10, Math.floor(minutes / 10), minutes % 10];
  for (let index = 0; index < digitGroups.length; index += 1) digitGroups[index].setDigit(digits[index], force);
  shownTimeText = `${pad2(hours)}:${pad2(minutes)}`;
}

function setMode(nextMode) {
  if (nextMode === clockMode) return;
  const current = resolveDisplayedTime(millis());
  if (nextMode !== "live") simulatedTime = floorToMinute(current);
  clockMode = nextMode;
  if (nextMode === "demo") lastDemoAdvanceAt = millis();
  if (nextMode === "live") applyDisplayedTime(new Date(), false);
  updateInterface(true);
}

function advanceOneMinute() {
  const current = resolveDisplayedTime(millis());
  simulatedTime = new Date(floorToMinute(current).getTime() + 60000);
  clockMode = "manual";
  applyDisplayedTime(simulatedTime, false);
  updateInterface(true);
}

function toggleDemo() { setMode(clockMode === "demo" ? "manual" : "demo"); }

function installControls() {
  document.getElementById("live-button").addEventListener("click", () => setMode("live"));
  document.getElementById("next-button").addEventListener("click", advanceOneMinute);
  document.getElementById("demo-button").addEventListener("click", toggleDemo);
  document.getElementById("scatter-button").addEventListener("click", scatterAll);
}

function installPointerInteraction() {
  function updatePointer(event) {
    const bounds = canvasElement.getBoundingClientRect();
    pointer.x = (event.clientX - bounds.left) * CONFIG.canvasWidth / bounds.width;
    pointer.y = (event.clientY - bounds.top) * CONFIG.canvasHeight / bounds.height;
  }
  canvasElement.addEventListener("pointerenter", event => { pointer.inside = true; updatePointer(event); });
  canvasElement.addEventListener("pointermove", event => { pointer.inside = true; updatePointer(event); }, { passive: true });
  canvasElement.addEventListener("pointerdown", event => {
    pointer.inside = true;
    updatePointer(event);
    try { canvasElement.setPointerCapture(event.pointerId); } catch (_) {}

    const touchExtraRadius = event.pointerType === "touch"
      ? CONFIG.antHitRadiusTouch - CONFIG.antHitRadiusMouse
      : 0;
    const hitAnt = findAntAt(pointer.x, pointer.y, touchExtraRadius);

    if (hitAnt) {
      pointer.down = false;
      pointer.inside = false;
      squashAnt(hitAnt, millis());
    } else {
      pointer.down = true;
      scatterAt(pointer.x, pointer.y, 112, 190);
    }
    event.preventDefault();
  }, { passive: false });
  canvasElement.addEventListener("pointerup", event => { updatePointer(event); pointer.down = false; });
  canvasElement.addEventListener("pointercancel", () => { pointer.down = false; pointer.inside = false; });
  canvasElement.addEventListener("pointerleave", () => { if (!pointer.down) pointer.inside = false; });
}

function installKeyboardControls() {
  window.addEventListener("keydown", event => {
    if (event.repeat) return;
    const key = event.key.toLowerCase();
    if (key === "l") setMode("live");
    else if (key === "n") advanceOneMinute();
    else if (key === "d") toggleDemo();
    else if (event.code === "Space") { scatterAll(); event.preventDefault(); }
    else if (key === "f") toggleFullscreen();
  });
}

function scatterAt(originX, originY, radius, power) {
  for (const ant of allAnts) {
    const dx = ant.x - originX, dy = ant.y - originY;
    const distance = Math.hypot(dx, dy);
    if (distance >= radius) continue;
    const angle = distance < 0.001 ? randomRange(0, Math.PI * 2) : Math.atan2(dy, dx);
    const impulse = power * (0.35 + (1 - distance / radius) * 0.95) * randomRange(0.82, 1.18);
    ant.addImpulse(Math.cos(angle) * impulse + randomRange(-18, 18), Math.sin(angle) * impulse + randomRange(-18, 18));
  }
}

function scatterAll() {
  const centerX = CONFIG.canvasWidth / 2, centerY = CONFIG.canvasHeight / 2;
  for (const ant of allAnts) {
    let dx = ant.x - centerX, dy = ant.y - centerY, distance = Math.hypot(dx, dy);
    if (distance < 1) { const angle = randomRange(0, Math.PI * 2); dx = Math.cos(angle); dy = Math.sin(angle); distance = 1; }
    const impulse = randomRange(105, 215);
    ant.addImpulse(dx / distance * impulse + randomRange(-68, 68), dy / distance * impulse + randomRange(-68, 68));
  }
}

function drawAmbientField(nowMs) {
  const pulse = 8 + 3 * Math.sin(nowMs * 0.0007);
  fill(145, 176, 157, pulse);
  rect(0, 49, CONFIG.canvasWidth, 1);
  rect(0, CONFIG.canvasHeight - 50, CONFIG.canvasWidth, 1);
}

function updateInterface(force) {
  const modeNames = { live: "LIVE", manual: "MANUAL", demo: "DEMO" };
  const assignedCount = allAnts.reduce((n, ant) => n + (ant.target ? 1 : 0), 0);
  const settledCount = allAnts.reduce((n, ant) => n + (ant.target && ant.settled ? 1 : 0), 0);
  document.getElementById("mode-badge").textContent = modeNames[clockMode];
  const squashStatus = squashedCount ? ` · ${squashedCount} squashed` : "";
  document.getElementById("status-text").textContent = `${shownTimeText} · ${settledCount}/${assignedCount} ants parked · ${allAnts.length} total${squashStatus}`;
  document.getElementById("live-button").classList.toggle("active", clockMode === "live");
  document.getElementById("demo-button").classList.toggle("active", clockMode === "demo");
  document.getElementById("demo-button").textContent = clockMode === "demo" ? "Stop demo" : "Auto demo";
  if (force) statusUpdatedAt = millis();
}

function toggleFullscreen() {
  const card = document.getElementById("fullscreen-card");
  if (!document.fullscreenElement) {
    const result = card.requestFullscreen();
    if (result && result.catch) result.catch(() => {});
  } else {
    const result = document.exitFullscreen();
    if (result && result.catch) result.catch(() => {});
  }
}

function floorToMinute(date) { const result = new Date(date); result.setSeconds(0, 0); return result; }
function pad2(value) { return String(value).padStart(2, "0"); }
function randomRange(minimum, maximum) { return minimum + Math.random() * (maximum - minimum); }
function clamp(value, minimum, maximum) { return Math.max(minimum, Math.min(maximum, value)); }
function lerpAngle(current, target, amount) { let delta = target - current; while (delta > Math.PI) delta -= Math.PI * 2; while (delta < -Math.PI) delta += Math.PI * 2; return current + delta * amount; }
