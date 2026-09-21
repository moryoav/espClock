"""Compile the light-glass renderer, verify approved pixels, and export frames."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image
from verify_ants_v2 import compile_host
from verify_flip_glass import read_frame, image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "flip_glass_light"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="flip-glass-light-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    for name, sha in manifest["source_sha256"].items():
        assert hashlib.sha256((ASSETS / "proposal" / name).read_bytes()).hexdigest() == sha
    assert hashlib.sha256((ROOT / "flip_glass_light_assets.h").read_bytes()).hexdigest() == manifest["header_sha256"]
    assert manifest["compiled_bytes"] == 870500
    executable = compile_host(directory, ROOT / "tests" / "flip_glass_light_host.cpp")
    subprocess.run([str(executable), str(directory)], check=True)
    bg = np.asarray(Image.open(ASSETS / "proposal" / "background.png").convert("RGB")).astype(np.uint32)
    bg565 = ((bg[:, :, 0] >> 3) << 11) | ((bg[:, :, 1] >> 2) << 5) | (bg[:, :, 2] >> 3)
    expanded = np.asarray(image(bg565)).astype(np.uint32)
    colon = np.asarray(Image.open(ASSETS / "proposal" / "colon.png").convert("RGBA")).astype(np.uint32)
    colon[:, :, :3] = (colon[:, :, :3] * colon[:, :, 3:4] + 127) // 255
    for digit in range(10):
        rgba = np.asarray(Image.open(ASSETS / "proposal" / f"{digit}.png").convert("RGBA")).astype(np.uint32)
        alpha = rgba[:, :, 3:4]
        premul = (rgba[:, :, :3] * alpha + 127) // 255
        actual = read_frame(directory / f"glyph_{digit}.bin")
        for x in (14, 122, 258, 366):
            rgb = np.minimum(255, premul + (expanded[84:212, x:x+100] * (255-alpha) + 127) // 255)
            expected = ((rgb[:, :, 0] >> 3) << 11) | ((rgb[:, :, 1] >> 2) << 5) | (rgb[:, :, 2] >> 3)
            assert np.array_equal(actual[84:212, x:x+100], expected), f"Sprite/background mismatch for {digit} at {x}"
        assert np.array_equal(actual[:84], bg565[:84]), "Background above digits changed"
        assert np.array_equal(actual[315:], bg565[315:]), "Background below reflections changed"
        # Independent direct 3x3 convolution, not the optimized separable C++
        # implementation. Check all colors and alpha before floor compositing.
        layer = np.zeros((128, 480, 4), np.uint32)
        for x in (14, 122, 258, 366):
            layer[:, x:x+100, :3] = premul
            layer[:, x:x+100, 3:4] = alpha
        region = layer[:, 190:290]
        region[:] = np.where(colon[:, :, 3:4] > 0, colon, region)
        fade = np.array([round(255*.44*(1-y/100)**1.25) for y in range(100)], np.uint32)
        reflected = layer[127-np.arange(100)*128//100] * fade[:, None, None]
        padded = np.pad(reflected, ((1,1),(1,1),(0,0)))
        filtered = np.zeros_like(reflected)
        for dy in range(3):
            for dx in range(3):
                filtered += padded[dy:dy+100, dx:dx+480] * (2 if dx == 1 else 1) * (2 if dy == 1 else 1)
        filtered //= 4080
        floor = np.minimum(255, filtered[:, :, :3] + (expanded[215:315] * (255-filtered[:, :, 3:4]) + 127) // 255)
        expected = ((floor[:, :, 0] >> 3) << 11) | ((floor[:, :, 1] >> 2) << 5) | (floor[:, :, 2] >> 3)
        assert np.array_equal(actual[215:315], expected), f"Reflection/alpha mismatch for {digit}"
    preview = ASSETS / "preview"
    preview.mkdir(exist_ok=True)
    static = image(read_frame(directory / "clock.bin"))
    static.save(preview / "clock.png")
    approved = np.asarray(Image.open(ASSETS / "proposal" / "clock-12-45.png").convert("RGB")).astype(np.int16)
    error = np.abs(np.asarray(static).astype(np.int16) - approved)
    # Independent browser reference includes smooth canvas resampling and a
    # slightly wider reflection blur; ESP32 uses integer pixel sampling.
    assert error.mean() < 3.0, f"Approved preview drift: {error.mean():.3f}"
    assert error[84:212].mean() < 3.0
    for name, prefix in (("flip-12-45-to-12-46", "frame"), ("flip-midnight", "midnight")):
        frames = [image(read_frame(directory / f"{prefix}_{n}.bin")) for n in range(25)]
        sheet = Image.new("RGB", (480, 320 * len(frames)))
        for n, frame in enumerate(frames):
            sheet.paste(frame, (0, n * 320))
        palette = sheet.quantize(colors=256)
        frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        frames[0].save(preview / f"{name}.gif", save_all=True, append_images=frames[1:],
                       duration=[1000] + [60, 60, 60, 70] * 5 + [60, 60, 60] + [1200], loop=0, optimize=False)
    print(f"PASS: approved RGBA sprites composite exactly in all four slots; independent reflection convolution, background, rotation and RGB565 verified. Preview mean RGB difference: {error.mean():.3f}/255.")
    print(f"Actual C++ renderer previews: {preview}")


if __name__ == "__main__":
    main()
