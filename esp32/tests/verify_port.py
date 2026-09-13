#!/usr/bin/env python3
"""Static parity checks between the approved browser prototype and ESP32 port.

Uses only the Python standard library. Run from any directory:
    python tests/verify_port.py
"""
from __future__ import annotations

import csv
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JS = (ROOT / "reference" / "html-sketch.js").read_text(encoding="utf-8")
CPP = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
LAYOUT_H = (ROOT / "include" / "digit_layouts.h").read_text(encoding="utf-8")
SPRITE_H = (ROOT / "include" / "ant_sprite.h").read_text(encoding="utf-8")
HARDWARE_H = (ROOT / "include" / "hardware_jc3248w535.h").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def fail(message: str) -> None:
    raise AssertionError(message)


def parse_js_layouts() -> dict[int, list[tuple[str, float, float]]]:
    block_match = re.search(
        r"const DIGIT_LAYOUTS = Object\.freeze\(\{(.*?)\n\}\);",
        JS,
        re.S,
    )
    if not block_match:
        fail("Could not locate DIGIT_LAYOUTS in reference HTML sketch")
    block = block_match.group(1)
    result: dict[int, list[tuple[str, float, float]]] = {}
    for match in re.finditer(r"^\s*(\d):\s*\[(.*?)^\s*\]", block, re.S | re.M):
        digit = int(match.group(1))
        result[digit] = [
            (kind, float(x), float(y))
            for kind, x, y in re.findall(r"([HV])\(([-\d.]+),\s*([-\d.]+)\)", match.group(2))
        ]
    return result


def parse_cpp_layouts() -> dict[int, list[tuple[str, float, float]]]:
    result: dict[int, list[tuple[str, float, float]]] = {}
    for match in re.finditer(
        r"constexpr Point kDigit(\d)\[\] = \{(.*?)\n\};",
        LAYOUT_H,
        re.S,
    ):
        digit = int(match.group(1))
        result[digit] = [
            (kind, float(x.rstrip("f")), float(y.rstrip("f")))
            for kind, x, y in re.findall(
                r"ANT_([HV])\(([-\d.]+f?),\s*([-\d.]+f?)\)",
                match.group(2),
            )
        ]
    return result


def parse_js_sprite_masks() -> list[list[str]]:
    match = re.search(
        r"const ANT_FRAME_MASKS = \[(.*?)\n\];\n\nconst ANT_PIXEL_COORDS",
        JS,
        re.S,
    )
    if not match:
        fail("Could not locate ANT_FRAME_MASKS in reference HTML sketch")
    frame_blocks = re.findall(r"\[(.*?)\]", match.group(1), re.S)
    frames: list[list[str]] = []
    for block in frame_blocks:
        rows = re.findall(r'"([.#]+)"', block)
        if rows:
            frames.append(rows)
    return frames


def parse_cpp_sprite_rows() -> list[list[int]]:
    match = re.search(
        r"constexpr uint16_t kRows\[kFrameCount\]\[kBodyHeight\] = \{(.*?)\n\};",
        SPRITE_H,
        re.S,
    )
    if not match:
        fail("Could not locate kRows in ant_sprite.h")
    frame_blocks = re.findall(r"\{(.*?)\}", match.group(1), re.S)
    frames: list[list[int]] = []
    for block in frame_blocks:
        values = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]+", block)]
        if values:
            frames.append(values)
    return frames


def js_mask_to_bits(frame: list[str]) -> list[int]:
    rows: list[int] = []
    for row in frame:
        bits = 0
        for x, value in enumerate(row):
            if value == "#":
                bits |= 1 << x
        rows.append(bits)
    return rows


def cpp_float(name: str) -> float:
    match = re.search(rf"constexpr float {re.escape(name)} = ([-\d.]+)f;", CPP)
    if not match:
        fail(f"Could not locate C++ float constant {name}")
    return float(match.group(1))


def cpp_uint(name: str) -> int:
    match = re.search(rf"constexpr (?:uint8_t|uint16_t|uint32_t) {re.escape(name)} = (\d+)", CPP)
    if not match:
        fail(f"Could not locate C++ integer constant {name}")
    return int(match.group(1))


def js_number(name: str) -> float:
    config = re.search(r"const CONFIG = Object\.freeze\(\{(.*?)\n\}\);", JS, re.S)
    if not config:
        fail("Could not locate CONFIG in reference HTML sketch")
    match = re.search(rf"^\s*{re.escape(name)}:\s*([-\d.]+)", config.group(1), re.M)
    if not match:
        fail(f"Could not locate JS CONFIG value {name}")
    return float(match.group(1))


def parse_int_literal(value: str) -> int:
    value = value.strip().rstrip(",")
    return int(value, 0)


def verify_partitions() -> None:
    partitions: list[tuple[str, int, int]] = []
    with (ROOT / "partitions.csv").open(encoding="utf-8", newline="") as handle:
        for row in csv.reader(line for line in handle if not line.lstrip().startswith("#")):
            if not row or not row[0].strip():
                continue
            name = row[0].strip()
            offset = parse_int_literal(row[3])
            size = parse_int_literal(row[4])
            partitions.append((name, offset, size))

    flash_size = 16 * 1024 * 1024
    previous_end = 0x9000
    for name, offset, size in sorted(partitions, key=lambda item: item[1]):
        if offset < previous_end:
            fail(f"Partition {name} overlaps a previous partition")
        if offset + size > flash_size:
            fail(f"Partition {name} exceeds 16 MB flash")
        previous_end = offset + size
    if previous_end != flash_size:
        fail(f"Partition table ends at 0x{previous_end:X}, expected exactly 0x1000000")


def main() -> int:
    js_layouts = parse_js_layouts()
    cpp_layouts = parse_cpp_layouts()
    if js_layouts != cpp_layouts:
        fail("C++ digit layouts no longer exactly match the approved HTML layouts")
    if set(cpp_layouts) != set(range(10)):
        fail("Digit layouts must contain all digits 0 through 9")

    expected_counts = {0: 12, 1: 5, 2: 11, 3: 11, 4: 10, 5: 11, 6: 12, 7: 7, 8: 13, 9: 12}
    actual_counts = {digit: len(points) for digit, points in cpp_layouts.items()}
    if actual_counts != expected_counts:
        fail(f"Unexpected digit ant counts: {actual_counts}")

    for digit, points in cpp_layouts.items():
        ys = [point[2] for point in points]
        if not math.isclose(min(ys), 0.0) or not math.isclose(max(ys), 1.0):
            fail(f"Digit {digit} does not span the common full height")
        if len({(kind, x, y) for kind, x, y in points}) != len(points):
            fail(f"Digit {digit} contains duplicate ant positions")
        for _, x, y in points:
            if not (0.0 <= x <= 1.0 and 0.0 <= y <= 1.0):
                fail(f"Digit {digit} contains an out-of-range normalized point")

    js_frames = parse_js_sprite_masks()
    cpp_frames = parse_cpp_sprite_rows()
    expected_frame_bits = [js_mask_to_bits(frame) for frame in js_frames]
    if expected_frame_bits != cpp_frames:
        fail("Embedded C++ ant sprites differ from the approved HTML sprite masks")
    if len(cpp_frames) != 2 or any(len(frame) != 19 for frame in cpp_frames):
        fail("Expected two 14x19 ant animation frames")

    parity = {
        "kDigitY": "digitY",
        "kDigitWidth": "digitWidth",
        "kDigitHeight": "digitHeight",
        "kTargetSpeed": "targetSpeed",
        "kIdleSpeedMin": "idleSpeedMin",
        "kIdleSpeedMax": "idleSpeedMax",
        "kPointerRadius": "pointerRadius",
        "kPointerForce": "pointerForce",
        "kActiveAntScale": "activeAntScale",
        "kIdleAntScale": "idleAntScale",
        "kSettledAntScale": "settledAntScale",
        "kHeadingSmoothing": "headingSmoothing",
        "kMicroJitter": "microJitter",
        "kAntHitRadius": "antHitRadiusMouse",
        "kTouchHitRadius": "antHitRadiusTouch",
        "kSeparationRadius": "separationRadius",
        "kSeparationForce": "separationForce",
        "kRespawnClearRadius": "respawnClearRadius",
        "kRespawnClearForce": "respawnClearForce",
        "kOverlapResolveDistance": "overlapResolveDistance",
    }
    for cpp_name, js_name in parity.items():
        if not math.isclose(cpp_float(cpp_name), js_number(js_name), rel_tol=0, abs_tol=1e-6):
            fail(f"Constant mismatch: {cpp_name} vs JS CONFIG.{js_name}")

    integer_parity = {
        "kDemoIntervalMs": "demoIntervalMs",
        "kWalkFrameMsMoving": "walkFrameMsMoving",
        "kWalkFrameMsIdle": "walkFrameMsIdle",
        "kWalkFrameMsSettled": "walkFrameMsSettled",
        "kBloodLifetimeMs": "bloodLifetimeMs",
        "kBloodFadeMs": "bloodFadeMs",
        "kMaximumBloodSpatters": "maximumBloodSpatters",
        "kReplacementProtectedMs": "replacementProtectedMs",
    }
    for cpp_name, js_name in integer_parity.items():
        if cpp_uint(cpp_name) != int(js_number(js_name)):
            fail(f"Constant mismatch: {cpp_name} vs JS CONFIG.{js_name}")

    required_hardware = {
        "kPanelWidth": 320,
        "kPanelHeight": 480,
        "kScreenWidth": 480,
        "kScreenHeight": 320,
        "kBacklightPin": 1,
        "kLcdCs": 45,
        "kLcdSck": 47,
        "kLcdD0": 21,
        "kLcdD1": 48,
        "kLcdD2": 40,
        "kLcdD3": 39,
        "kTouchSda": 4,
        "kTouchScl": 8,
    }
    for name, expected in required_hardware.items():
        match = re.search(rf"constexpr (?:int16_t|int8_t) {name} = (-?\d+);", HARDWARE_H)
        if not match or int(match.group(1)) != expected:
            fail(f"Unexpected hardware profile value for {name}")

    if "platform = espressif32 @ 6.13.0" not in PIO:
        fail("PlatformIO ESP32 platform is not pinned")
    if "GFX Library for Arduino@1.6.0" not in PIO:
        fail("Arduino_GFX must remain pinned to the tested 1.6.0 board example")
    if "board_build.arduino.memory_type = qio_opi" not in PIO or "BOARD_HAS_PSRAM" not in PIO:
        fail("PSRAM build configuration is missing")

    verify_partitions()

    required_features = [
        "createBloodSpatter",
        "computeAntSeparation",
        "resolveAntOverlaps",
        "respawnReplacement",
        "readTouchPoint",
        "configTzTime",
        "kFireAntRgb{232, 61, 39}",
    ]
    missing = [feature for feature in required_features if feature not in CPP]
    if missing:
        fail(f"Missing expected port features: {', '.join(missing)}")

    print("PASS: HTML/C++ digit layouts match exactly")
    print("PASS: two 14x19 ant sprite frames match exactly")
    print("PASS: animation/interaction constants match the browser prototype")
    print("PASS: all digits span the same normalized height")
    print("PASS: hardware profile, PSRAM config, and 16 MB partitions are consistent")
    print("PASS: fire ants, touch squashing, blood, respawn, and collision avoidance are present")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
