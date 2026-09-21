"""Compile the Pizza renderer, check behavior and export firmware previews."""
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
ASSETS = ROOT / "assets/pizza"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="pizza-clock-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    for name, sha in manifest["source_sha256"].items():
        assert hashlib.sha256((ASSETS / "proposal" / name).read_bytes()).hexdigest() == sha
    assert hashlib.sha256((ROOT / "pizza_assets.h").read_bytes()).hexdigest() == manifest["header_sha256"]
    assert manifest["compiled_bytes"] < 1500000
    executable = compile_host(directory, ROOT / "tests/pizza_host.cpp")
    subprocess.run([str(executable), str(directory)], check=True)
    preview = ASSETS / "preview"
    preview.mkdir(exist_ok=True)
    static = image(read_frame(directory / "clock.bin"))
    static.save(preview / "clock.png")
    approved = np.asarray(Image.open(ASSETS / "proposal/clock-12-45.png").convert("RGB")).astype(np.int16)
    error = np.abs(np.asarray(static).astype(np.int16) - approved)
    assert error.mean() < 3.0, f"Approved preview drift: {error.mean():.3f}"
    assert error[80:240].mean() < 5.0
    bg = np.asarray(Image.open(ASSETS / "proposal/background.png").convert("RGB")).astype(np.uint16)
    bg565 = ((bg[:,:,0] >> 3) << 11) | ((bg[:,:,1] >> 2) << 5) | (bg[:,:,2] >> 3)
    for n in range(10):
        actual = read_frame(directory / f"glyph_{n}.bin")
        assert np.array_equal(actual[:65], bg565[:65])
        assert np.array_equal(actual[250:], bg565[250:])
    for name, prefix in (("eat-and-rebuild", "frame"), ("midnight", "midnight")):
        frames = [image(read_frame(directory / f"{prefix}_{n}.bin")) for n in range(61)]
        sheet = Image.new("RGB", (480, 320*len(frames)))
        for n, frame in enumerate(frames):
            sheet.paste(frame, (0, n*320))
        palette = sheet.quantize(colors=256)
        frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        frames[0].save(preview / f"{name}.gif", save_all=True, append_images=frames[1:],
                       duration=[1000]+[60]*59+[1200], loop=0, optimize=False)
    print(f"PASS: artwork hashes, flash budget, untouched cheese and approved preview (mean RGB error {error.mean():.3f}/255).")
    print(f"Firmware previews: {preview}")


if __name__ == "__main__":
    main()
