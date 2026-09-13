/*
 * p5-lite.js
 * Tiny dependency-free compatibility layer implementing only the p5 global
 * functions used by this prototype. This is project code, not official p5.js.
 */
(function () {
  "use strict";

  let canvas = null;
  let ctx = null;
  let startedAt = performance.now();
  let previousFrameAt = startedAt;
  let lastRenderedAt = 0;
  let requestedFrameRate = 60;
  let currentFill = "rgba(255,255,255,1)";
  const activeTouches = new Map();

  Object.assign(window, {
    width: 0,
    height: 0,
    frameCount: 0,
    deltaTime: 1000 / 60,
    mouseX: -100000,
    mouseY: -100000,
    pmouseX: -100000,
    pmouseY: -100000,
    mouseIsPressed: false,
    touches: [],
    TWO_PI: Math.PI * 2
  });

  const byte = value => Math.max(0, Math.min(255, Number(value) || 0));

  function cssColor(args) {
    const v = Array.from(args);
    if (!v.length) return "rgba(255,255,255,1)";
    if (typeof v[0] === "string") return v[0];
    let r, g, b, a = 255;
    if (v.length === 1) r = g = b = byte(v[0]);
    else if (v.length === 2) { r = g = b = byte(v[0]); a = byte(v[1]); }
    else { r = byte(v[0]); g = byte(v[1]); b = byte(v[2]); if (v.length > 3) a = byte(v[3]); }
    return `rgba(${r},${g},${b},${a / 255})`;
  }

  function requireCanvas() {
    if (!canvas || !ctx) throw new Error("createCanvas() must be called before drawing.");
  }

  function pointerCoordinates(event) {
    if (!canvas) return { x: -100000, y: -100000 };
    const bounds = canvas.getBoundingClientRect();
    if (!bounds.width || !bounds.height) return { x: -100000, y: -100000 };
    return {
      x: (event.clientX - bounds.left) * canvas.width / bounds.width,
      y: (event.clientY - bounds.top) * canvas.height / bounds.height
    };
  }

  function updateMouse(event) {
    const point = pointerCoordinates(event);
    window.pmouseX = window.mouseX;
    window.pmouseY = window.mouseY;
    window.mouseX = point.x;
    window.mouseY = point.y;
    return point;
  }

  function refreshTouches() {
    window.touches = Array.from(activeTouches.entries()).map(([id, p]) => ({
      id, x: p.x, y: p.y, winX: p.winX, winY: p.winY
    }));
  }

  function callOptional(name, event) {
    if (typeof window[name] !== "function") return;
    const result = window[name](event);
    if (result === false && event && event.preventDefault) event.preventDefault();
  }

  function installPointerEvents() {
    canvas.addEventListener("contextmenu", event => event.preventDefault());
    canvas.addEventListener("pointerenter", updateMouse);
    canvas.addEventListener("pointermove", event => {
      const point = updateMouse(event);
      if (event.pointerType === "touch" && activeTouches.has(event.pointerId)) {
        activeTouches.set(event.pointerId, { x: point.x, y: point.y, winX: event.clientX, winY: event.clientY });
        refreshTouches();
        callOptional("touchMoved", event);
      } else callOptional("mouseMoved", event);
    }, { passive: false });

    canvas.addEventListener("pointerdown", event => {
      const point = updateMouse(event);
      window.mouseIsPressed = true;
      try { canvas.setPointerCapture(event.pointerId); } catch (_) {}
      if (event.pointerType === "touch") {
        activeTouches.set(event.pointerId, { x: point.x, y: point.y, winX: event.clientX, winY: event.clientY });
        refreshTouches();
        callOptional("touchStarted", event);
      } else callOptional("mousePressed", event);
      event.preventDefault();
    }, { passive: false });

    function finish(event, cancelled) {
      updateMouse(event);
      if (event.pointerType === "touch") {
        activeTouches.delete(event.pointerId);
        refreshTouches();
        callOptional("touchEnded", event);
      } else callOptional("mouseReleased", event);
      window.mouseIsPressed = activeTouches.size > 0;
      if (cancelled || event.pointerType === "touch") {
        window.mouseX = -100000;
        window.mouseY = -100000;
      }
    }

    canvas.addEventListener("pointerup", event => finish(event, false), { passive: false });
    canvas.addEventListener("pointercancel", event => finish(event, true), { passive: false });
    canvas.addEventListener("pointerleave", event => {
      if (!window.mouseIsPressed) { window.mouseX = -100000; window.mouseY = -100000; }
      callOptional("mouseOut", event);
    });
  }

  window.createCanvas = function createCanvas(canvasWidth, canvasHeight) {
    canvas = document.createElement("canvas");
    canvas.width = Math.max(1, Math.floor(canvasWidth));
    canvas.height = Math.max(1, Math.floor(canvasHeight));
    canvas.className = "p5Canvas";
    canvas.setAttribute("role", "img");
    canvas.setAttribute("aria-label", "Animated dot clock");
    ctx = canvas.getContext("2d", { alpha: false });
    window.width = canvas.width;
    window.height = canvas.height;
    document.body.appendChild(canvas);
    installPointerEvents();

    const renderer = {
      elt: canvas,
      parent(parentOrId) {
        const parent = typeof parentOrId === "string" ? document.getElementById(parentOrId) : parentOrId;
        if (!parent) throw new Error(`Canvas parent not found: ${parentOrId}`);
        parent.appendChild(canvas);
        return renderer;
      }
    };
    return renderer;
  };

  window.resizeCanvas = function resizeCanvas(w, h) {
    requireCanvas();
    canvas.width = Math.max(1, Math.floor(w));
    canvas.height = Math.max(1, Math.floor(h));
    window.width = canvas.width;
    window.height = canvas.height;
  };

  window.pixelDensity = () => 1;
  window.frameRate = function frameRate(value) {
    if (value === undefined) return window.deltaTime > 0 ? 1000 / window.deltaTime : requestedFrameRate;
    requestedFrameRate = Math.max(1, Number(value) || 60);
    return requestedFrameRate;
  };
  window.noStroke = () => {};
  window.fill = function fill() { currentFill = cssColor(arguments); if (ctx) ctx.fillStyle = currentFill; };
  window.background = function background() {
    requireCanvas();
    const old = ctx.fillStyle;
    ctx.fillStyle = cssColor(arguments);
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = old || currentFill;
  };
  window.clear = function clear() { requireCanvas(); ctx.clearRect(0, 0, canvas.width, canvas.height); };
  window.ellipse = function ellipse(x, y, w, h) {
    requireCanvas();
    const actualH = h === undefined ? w : h;
    ctx.beginPath();
    ctx.ellipse(x, y, Math.abs(w) / 2, Math.abs(actualH) / 2, 0, 0, Math.PI * 2);
    ctx.fillStyle = currentFill;
    ctx.fill();
  };
  window.circle = (x, y, diameter) => window.ellipse(x, y, diameter, diameter);
  window.rect = function rect(x, y, w, h) { requireCanvas(); ctx.fillStyle = currentFill; ctx.fillRect(x, y, w, h); };
  window.random = function random(minimum, maximum) {
    if (minimum === undefined) return Math.random();
    if (Array.isArray(minimum)) return minimum[Math.floor(Math.random() * minimum.length)];
    if (maximum === undefined) return Math.random() * minimum;
    return minimum + Math.random() * (maximum - minimum);
  };
  window.millis = () => performance.now() - startedAt;
  window.fullscreen = function fullscreen(value) {
    if (value === undefined) return Boolean(document.fullscreenElement);
    if (value && !document.fullscreenElement) return (canvas || document.documentElement).requestFullscreen();
    if (!value && document.fullscreenElement) return document.exitFullscreen();
    return Promise.resolve();
  };

  function animationFrame(now) {
    const minimumInterval = 1000 / requestedFrameRate;
    if (now - lastRenderedAt + 0.2 < minimumInterval) {
      requestAnimationFrame(animationFrame);
      return;
    }
    window.deltaTime = Math.max(0.1, Math.min(100, now - previousFrameAt));
    previousFrameAt = now;
    lastRenderedAt = now;
    window.frameCount += 1;
    if (typeof window.draw === "function") window.draw();
    requestAnimationFrame(animationFrame);
  }

  function start() {
    startedAt = performance.now();
    previousFrameAt = startedAt;
    if (typeof window.setup === "function") window.setup();
    requestAnimationFrame(animationFrame);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start, { once: true });
  else queueMicrotask(start);
})();
