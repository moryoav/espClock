"""Bake the ants_v2 animation rig into PNG atlases before compiling firmware.

The image-generation artwork is in assets/ants_v2/source. This offline pipeline
articulates that artwork, supersamples the rotations, and packs the final pixels.
No rigging, scaling, or rotation is performed on the ESP32.
"""

from __future__ import annotations

import hashlib
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "ants_v2"
SOURCE = ASSETS / "source"
FRAMES = 12
DIRECTIONS = 64
CANVAS = 512
WORLD = 32
S = CANVAS / WORLD
ATLAS_WIDTH = 1024
BANKS = (("gold", 1.0), ("idle", 0.72), ("fire", 1.0))
LEG_LENGTH_SCALE = 0.86


def rgba(path: Path) -> Image.Image:
    image = Image.open(path)
    if image.mode != "RGBA" or image.getchannel("A").getextrema()[0] != 0:
        raise ValueError(f"{path.name} must have genuine alpha transparency")
    # Generated cutouts can contain nearly invisible alpha specks far outside
    # the subject. Remove only 1..3/255 noise before deriving the rig bounds.
    image.putalpha(image.getchannel("A").point(lambda value: 0 if value < 4 else value))
    return image


def pixel(point):
    return np.asarray(point, dtype=float) * S + CANVAS / 2


def place_segment(canvas, image, source_a, source_b, target_a, target_b, thickness=0.60):
    """Rigid textured limb transformation, with no independently redrawn frames."""
    sa, sb = np.asarray(source_a, float), np.asarray(source_b, float)
    ta, tb = pixel(target_a), pixel(target_b)
    sv, tv = sb - sa, tb - ta
    scale = np.linalg.norm(tv) / np.linalg.norm(sv)
    # PIL transforms use the inverse mapping: destination -> original artwork.
    su, tu = sv / np.linalg.norm(sv), tv / np.linalg.norm(tv)
    sp, tp = np.array([-su[1], su[0]]), np.array([-tu[1], tu[0]])
    inverse = np.outer(su, tu) / scale + np.outer(sp, tp) / (scale * thickness)
    offset = sa - inverse @ ta
    coeffs = (*inverse[0], offset[0], *inverse[1], offset[1])
    part = image.transform(
        (CANVAS, CANVAS), Image.Transform.AFFINE, coeffs,
        resample=Image.Resampling.BICUBIC,
    )
    canvas.alpha_composite(part)


def split_leg(leg):
    # Source art joint coordinates, retained in rig.json for reproducibility.
    upper, lower = leg.copy(), leg.copy()
    upper_alpha = np.array(upper.getchannel("A"))
    lower_alpha = np.array(lower.getchannel("A"))
    upper_alpha[480:] = 0
    lower_alpha[:390] = 0
    upper.putalpha(Image.fromarray(upper_alpha))
    lower.putalpha(Image.fromarray(lower_alpha))
    return upper, lower


def knee_position(hip, foot, upper_length, lower_length, preferred):
    hip, foot = np.asarray(hip, float), np.asarray(foot, float)
    delta = foot - hip
    distance = np.linalg.norm(delta)
    if not abs(upper_length - lower_length) < distance < upper_length + lower_length:
        raise ValueError("Unreachable foot target in animation rig")
    unit = delta / distance
    along = (upper_length**2 - lower_length**2 + distance**2) / (2 * distance)
    height = math.sqrt(max(0, upper_length**2 - along**2))
    perpendicular = np.array([-unit[1], unit[0]])
    candidates = (hip + along * unit + height * perpendicular,
                  hip + along * unit - height * perpendicular)
    return min(candidates, key=lambda p: np.linalg.norm(p - preferred))


def make_frame(phase, body, upper, lower, antenna):
    canvas = Image.new("RGBA", (CANVAS, CANVAS))
    # Hips attach to the thorax, not to the abdomen. Opposite tripods alternate:
    # left-front/right-middle/left-rear versus right-front/left-middle/right-rear.
    hips_y = (-3.2, -1.8, -0.4)
    feet_y = (-8.3, 0.1, 7.8)
    feet_x = (7.0, 8.8, 7.0)
    lengths = ((5.8, 4.8), (5.8, 5.0), (6.0, 5.2))
    preferred_knees = ((5.0, -5.5), (6.0, -3.5), (5.5, 3.6))
    for side in (-1, 1):
        for index in range(3):
            offset = 0 if (side == -1) == (index != 1) else math.pi
            theta = phase + offset
            hip = (side * 1.65, hips_y[index])
            # Smooth cyclic reach and recovery. The recovery tripod retracts
            # slightly sideways, bending its knees rather than sliding rigid legs.
            foot = (side * (feet_x[index] - 0.8 * max(0, math.cos(theta))),
                    feet_y[index] + 1.25 * math.sin(theta))
            preferred = (side * preferred_knees[index][0], preferred_knees[index][1])
            knee = knee_position(hip, foot, *lengths[index], preferred)
            # Shorten both articulated segments around the fixed body joint.
            # Preserve the tripod gait and knee angles, with a subtler reach.
            knee = tuple(h + LEG_LENGTH_SCALE * (k - h) for h, k in zip(hip, knee))
            foot = tuple(h + LEG_LENGTH_SCALE * (f - h) for h, f in zip(hip, foot))
            place_segment(canvas, upper, (194, 137), (614, 442), hip, knee)
            place_segment(canvas, lower, (614, 442), (1095, 1159), knee, foot)

    for side in (-1, 1):
        base = (side * 1.25, -6.0)
        tip = (side * (4.4 + 0.30 * math.sin(phase + side * 0.55)),
               -12.2 + 0.20 * math.cos(phase + side * 0.55))
        place_segment(canvas, antenna, (1050, 1074), (182, 180), base, tip, thickness=0.75)

    # The body is an identical image at an identical position in every frame.
    # It covers the limb roots, preserving clean joins during articulation.
    height = round(17 * S)
    width = round(height * body.width / body.height)
    body_scaled = body.resize((width, height), Image.Resampling.LANCZOS)
    canvas.alpha_composite(body_scaled, (round(CANVAS / 2 - width / 2), round(8 * S)))
    return canvas


def color_variant(image, bank):
    if bank == "gold":
        return image
    pixels = np.array(image)
    rgb = pixels[:, :, :3].astype(np.float32)
    if bank == "fire":
        # Keep the generated shell's shading, but use a warm red colon palette.
        rgb[:, :, 0] = np.minimum(255, rgb[:, :, 0] * 1.04)
        rgb[:, :, 1] *= 0.40
        rgb[:, :, 2] *= 0.40
    else:
        pixels[:, :, 3] = ((pixels[:, :, 3].astype(np.uint16) * 150 + 127) // 255).astype(np.uint8)
    pixels[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    return Image.fromarray(pixels)


def pack_bank(frames, name, scale):
    tiles = []
    for frame_index, frame in enumerate(frames):
        colored = color_variant(frame, name)
        for direction in range(DIRECTIONS):
            # Runtime angles are clockwise from "head up". After rotating in
            # logical coordinates, bake the panel's native 90-degree mapping too.
            rotated = colored.rotate(-360 * direction / DIRECTIONS,
                                     Image.Resampling.BICUBIC, expand=False)
            size = round(WORLD * scale)
            small = rotated.resize((size, size), Image.Resampling.LANCZOS)
            small = small.transpose(Image.Transpose.ROTATE_270)
            bounds = small.getchannel("A").getbbox()
            if bounds is None:
                raise ValueError("Empty sprite")
            crop = small.crop(bounds)
            dx = bounds[0] - (size - 1 - size // 2)
            dy = bounds[1] - size // 2
            tiles.append((frame_index * DIRECTIONS + direction, crop, dx, dy))

    # Deterministic shelf packing, with stable frame/direction lookup records.
    tiles.sort(key=lambda t: (-t[1].height, -t[1].width, t[0]))
    records = [None] * (FRAMES * DIRECTIONS)
    placements = []
    x, y, row_height = 0, 0, 0
    for index, tile, dx, dy in tiles:
        if x + tile.width > ATLAS_WIDTH:
            x, y, row_height = 0, y + row_height, 0
        records[index] = [x, y, tile.width, tile.height, dx, dy]
        placements.append((tile, x, y))
        x += tile.width
        row_height = max(row_height, tile.height)
    atlas = Image.new("RGBA", (ATLAS_WIDTH, y + row_height))
    for tile, x, y in placements:
        atlas.paste(tile, (x, y))
    atlas.save(ASSETS / f"atlas_{name}.png", optimize=True)
    return {"name": name, "width": atlas.width, "height": atlas.height,
            "scale": scale, "sprites": records,
            "compiled_bytes": atlas.width * atlas.height * 3}


def write_header(banks):
    lines = [
        "// Generated by tools/build_ants_v2_sprites.py. Do not edit by hand.",
        "#pragma once", "#include <cstdint>", "namespace ants_v2_assets {",
        f"constexpr uint8_t kFrames = {FRAMES};",
        f"constexpr uint8_t kDirections = {DIRECTIONS};",
        "constexpr uint8_t kBanks = 3;",
        "struct Sprite { uint16_t x, y; uint8_t width, height; int8_t dx, dy; };",
        "constexpr uint16_t kAtlasWidths[kBanks] = {" + ", ".join(str(b["width"]) for b in banks) + "};",
        "constexpr uint16_t kAtlasHeights[kBanks] = {" + ", ".join(str(b["height"]) for b in banks) + "};",
        "constexpr Sprite kSprites[kBanks][kFrames * kDirections] = {",
    ]
    for bank in banks:
        lines.append("  { // " + bank["name"])
        lines.extend("    {" + ", ".join(map(str, record)) + "}," for record in bank["sprites"])
        lines.append("  },")
    lines += ["};", "}  // namespace ants_v2_assets", ""]
    (ROOT / "ants_v2_assets.h").write_text("\n".join(lines), encoding="utf-8")


def write_previews(frames):
    preview = ASSETS / "preview"
    preview.mkdir(exist_ok=True)
    sheet = Image.new("RGB", (6 * 192, 2 * 210), (5, 11, 13))
    draw = ImageDraw.Draw(sheet)
    for index, frame in enumerate(frames):
        thumb = frame.resize((192, 192), Image.Resampling.LANCZOS)
        x, y = index % 6 * 192, index // 6 * 210
        sheet.paste(thumb, (x, y), thumb)
        draw.text((x + 8, y + 190), f"{index + 1:02d}", fill=(205, 213, 209))
    sheet.save(preview / "walking_frames.png")
    animated = []
    for frame in frames:
        bg = Image.new("RGBA", (CANVAS, CANVAS), (5, 11, 13, 255))
        bg.alpha_composite(frame)
        animated.append(bg.convert("RGB").resize((320, 320), Image.Resampling.LANCZOS))
    animated[0].save(preview / "walking.gif", save_all=True,
                     append_images=animated[1:], duration=70, loop=0)

    # Actual-size and nearest-neighbor enlarged previews expose leg readability.
    sheet = Image.new("RGB", (480, 235), (5, 11, 13))
    draw = ImageDraw.Draw(sheet)
    for i, frame in enumerate(frames):
        small = frame.resize((32, 32), Image.Resampling.LANCZOS)
        sheet.paste(small, (i * 40 + 4, 20), small)
        big = small.resize((80, 80), Image.Resampling.NEAREST)
        sheet.paste(big, ((i % 6) * 80, 65 + (i // 6) * 85), big)
    draw.text((4, 3), "Actual size / enlarged pixels", fill=(205, 213, 209))
    sheet.save(preview / "actual_size.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview-only", action="store_true")
    args = parser.parse_args()
    ASSETS.mkdir(parents=True, exist_ok=True)
    body = rgba(SOURCE / "body.png")
    bounds = body.getchannel("A").point(lambda value: 255 if value >= 8 else 0).getbbox()
    body = body.crop((bounds[0] - 4, bounds[1] - 4, bounds[2] + 4, bounds[3] + 4))
    leg = rgba(SOURCE / "leg.png")
    antenna = rgba(SOURCE / "antenna.png")
    upper, lower = split_leg(leg)
    frames = [make_frame(2 * math.pi * i / FRAMES, body, upper, lower, antenna)
              for i in range(FRAMES)]
    frame_dir = ASSETS / "walk"
    frame_dir.mkdir(exist_ok=True)
    for i, frame in enumerate(frames):
        frame.save(frame_dir / f"frame_{i:02d}.png", optimize=True)
    # Closure is a property of the rig, rather than relying on AI to match frames.
    if not np.array_equal(np.array(frames[0]), np.array(make_frame(2 * math.pi, body, upper, lower, antenna))):
        raise AssertionError("Animation loop does not close exactly")
    write_previews(frames)
    if args.preview_only:
        print("Walking frames and previews generated.")
        return
    banks = [pack_bank(frames, name, scale) for name, scale in BANKS]
    write_header(banks)
    manifest = {
        "frames": FRAMES, "directions": DIRECTIONS,
        "coordinate_system": "native panel pixels; clockwise logical directions from head-up",
        "banks": banks,
        "source_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(SOURCE.glob("*.png"))},
        "atlas_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in sorted(ASSETS.glob("atlas_*.png"))},
        "rig": {"canvas": CANVAS, "world": WORLD, "leg_length_scale": LEG_LENGTH_SCALE, "hip": [194,137],
                "knee": [614,442], "foot": [1095,1159],
                "antenna_base": [1050,1074], "antenna_tip": [182,180]},
    }
    (ASSETS / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"frames": FRAMES, "directions": DIRECTIONS,
                      "sprites": FRAMES * DIRECTIONS * len(BANKS),
                      "compiled_atlas_bytes": sum(b["compiled_bytes"] for b in banks),
                      "atlases": [{k: b[k] for k in ("name", "width", "height")} for b in banks]}, indent=2))


if __name__ == "__main__":
    main()
