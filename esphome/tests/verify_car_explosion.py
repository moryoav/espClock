"""Compare the native explosion blitter with the approved Canvas frame PNGs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from PIL import Image
from verify_ants_v2 import compile_host
from verify_flip_glass import read_frame

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/cars/explosion"

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def blend(reference, sprite, x, y):
    rgba = np.asarray(sprite).astype(np.uint32)
    h, w, _ = rgba.shape
    left, top = x - w // 2, y - h // 2
    x0, y0, x1, y1 = max(0, left), max(0, top), min(480, left+w), min(320, top+h)
    if x0 >= x1 or y0 >= y1:
        return
    src = rgba[y0-top:y1-top, x0-left:x1-left]
    destination = reference[y0:y1, x0:x1].astype(np.uint32)
    r5, g6, b5 = destination >> 11, (destination >> 5) & 63, destination & 31
    rgb = np.stack(((r5*255+15)//31, (g6*255+31)//63, (b5*255+15)//31), axis=-1)
    alpha = src[:, :, 3:4]
    result = (src[:, :, :3]*alpha + rgb*(255-alpha) + 127)//255
    packed = ((result[:, :, 0] >> 3) << 11) | ((result[:, :, 1] >> 2) << 5) | (result[:, :, 2] >> 3)
    reference[y0:y1, x0:x1] = np.where(alpha[:, :, 0] >= 3, packed, destination)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="car-explosion-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    assert manifest["compiled_bytes"] < 256*1024
    assert manifest["duration_ms"] == 350 and manifest["impact_delay_ms"] == 90
    assert digest(ASSETS / "approved-preview.html") == manifest["approved_preview_sha256"]
    assert digest(ROOT / "tools/car_explosion_art.cjs") == manifest["source_sha256"]
    assert digest(ROOT / "car_explosion_assets.h") == manifest["header_sha256"]
    assert digest(ASSETS / "atlas.png") == manifest["atlas_sha256"]
    assert (ASSETS / "atlas.png").read_bytes() == (ROOT.parent / "pc/assets/car_explosion.png").read_bytes()
    for item in manifest["images"]:
        assert digest(ASSETS / item["file"]) == item["sha256"]
        sprite = Image.open(ASSETS / item["file"])
        assert sprite.mode == "RGBA" and sprite.size == (item["size"], item["size"])
        assert sprite.getchannel("A").getextrema()[0] == 0
    executable = compile_host(directory, ROOT / "tests/car_explosion_host.cpp")
    subprocess.run([str(executable), str(directory)], check=True)
    index = 0
    background = (18 >> 3) << 11 | (21 >> 2) << 5 | (23 >> 3)
    for bank in ("digit", "colon"):
        for frame in range(6):
            sprite = Image.open(ASSETS / f"{bank}-{frame}.png")
            for x, y in ((240,160), (0,0), (479,319), (0,319), (479,0), (-500,-500), (980,820)):
                expected = np.full((320,480), background, np.uint16)
                blend(expected, sprite, x, y)
                actual = read_frame(directory / f"explosion-{index}.bin")
                assert np.array_equal(actual, expected), (bank, frame, x, y, np.count_nonzero(actual != expected))
                index += 1
    expected = np.full((320,480), background, np.uint16)
    blend(expected, Image.open(ASSETS / "digit-2.png"), 240,160)
    blend(expected, Image.open(ASSETS / "colon-3.png"), 267,153)
    assert np.array_equal(read_frame(directory / "overlap.bin"), expected)
    print("PASS: approved-source hashes, PC/firmware artwork agreement, exact RGBA blending and native orientation for all 84 cases plus overlap.")

if __name__ == "__main__":
    main()
