"""Compile the actual Flip Glass C++ renderer, test it, and export its frames."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image
from verify_ants_v2 import compile_host

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "flip_glass"


def read_frame(path):
    # Independent inverse of the JC3248W535 physical framebuffer rotation.
    native = np.frombuffer(path.read_bytes(), dtype=">u2").reshape(480, 320)
    return np.rot90(native).copy()


def image(pixels):
    values = pixels.astype(np.uint32)
    return Image.fromarray(np.stack((
        ((values >> 11) & 31) * 255 // 31,
        ((values >> 5) & 63) * 255 // 63,
        (values & 31) * 255 // 31,
    ), axis=-1).astype(np.uint8))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="flip-glass-test-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    assert hashlib.sha256((ASSETS / "approved-design.png").read_bytes()).hexdigest() == manifest["source_sha256"]
    assert hashlib.sha256((ASSETS / "digits.png").read_bytes()).hexdigest() == manifest["atlas_sha256"]
    assert manifest["compiled_bytes"] == 281600
    executable = compile_host(directory, ROOT / "tests" / "flip_glass_host.cpp")
    subprocess.run([str(executable), str(directory)], check=True)
    # Verify real C++ pixels against the PNGs at rest, independent of C++ sampling.
    for digit in range(10):
        rgb = np.asarray(Image.open(ASSETS / f"{digit}.png")).astype(np.uint16)
        expected = ((rgb[:, :, 0] >> 3) << 11) | ((rgb[:, :, 1] >> 2) << 5) | (rgb[:, :, 2] >> 3)
        actual = read_frame(directory / f"glyph_{digit}.bin")
        for x in (14, 122, 258, 366):
            assert np.array_equal(actual[84:212, x:x+100], expected), f"Sprite/rotation mismatch for {digit}"
    preview = ASSETS / "preview"
    preview.mkdir(exist_ok=True)
    image(read_frame(directory / "clock.bin")).save(preview / "clock.png")
    for name, prefix in (("flip-12-45-to-12-46", "frame"), ("flip-midnight", "midnight")):
        frames = [image(read_frame(directory / f"{prefix}_{n}.bin")) for n in range(25)]
        # Single palette across the animation avoids distracting palette flicker.
        sheet = Image.new("RGB", (480, 320 * len(frames)))
        for n, frame in enumerate(frames):
            sheet.paste(frame, (0, n * 320))
        palette = sheet.quantize(colors=256)
        frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        frames[0].save(preview / f"{name}.gif", save_all=True, append_images=frames[1:],
                       duration=[1000] + [60, 60, 60, 70] * 5 + [60, 60, 60] + [1200], loop=0, optimize=False)
    print("PASS: all ten glyphs match approved sprites pixel-for-pixel in all four slots; RGB565 and physical rotation verified.")
    print(f"Actual C++ renderer previews: {preview}")


if __name__ == "__main__":
    main()
