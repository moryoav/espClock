"""Exercise the actual Cars renderer and export its tap animation for review."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image
from verify_ants_v2 import compile_host
from verify_flip_glass import image, read_frame

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="cars-test-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / "assets/cars/manifest.json").read_text())
    for name, filename in (("normal", "car.png"), ("damaged", "car_crushed.png")):
        path = ROOT / "assets" / filename
        sprite = Image.open(path)
        assert sprite.mode == "RGBA" and sprite.size == (28, 52)
        (directory / f"{name}.rgba").write_bytes(sprite.tobytes())
        if name == "damaged":
            assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest["sprite_sha256"]
            assert path.read_bytes() == (ROOT.parent / "pc/assets" / filename).read_bytes()
            alpha = np.asarray(sprite)[:, :, 3]
            assert not alpha[:, 0].any() and not alpha[:, -1].any()
            assert alpha[15:40, 12:16].min() > 220, "Dark windows must remain opaque"
    executable = compile_host(directory, ROOT / "tests/cars_host.cpp")
    subprocess.run([str(executable), str(directory)], check=True)
    preview = ROOT / "assets/cars/preview"
    preview.mkdir(exist_ok=True)
    before = image(read_frame(directory / "before.bin"))
    before.save(preview / "before.png")
    frames = [image(read_frame(directory / f"frame_{n}.bin")) for n in range(101)]
    frames[3].save(preview / "explosion.png")
    frames[8].save(preview / "crushed.png")
    # One palette prevents flicker between states at the display's actual size.
    sheet = Image.new("RGB", (480, 320 * (len(frames) + 1)))
    for n, frame in enumerate([before] + frames):
        sheet.paste(frame, (0, n * 320))
    palette = sheet.quantize(colors=256)
    gif_frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in [before] + frames]
    gif_frames[0].save(preview / "tap-replacement.gif", save_all=True, append_images=gif_frames[1:],
                       duration=[800] + [80] * 100 + [1000], loop=0, optimize=False)
    # The tapped cars restore their original appearance. Other cars can turn
    # around while yielding, as in the existing renderer; C++ checks every pose.
    for box in ((434, 92, 466, 149), (391, 209, 449, 240)):
        assert frames[-1].crop(box).tobytes() == before.crop(box).tobytes(), "Replacement sprite mismatch"
    print(f"PASS: RGBA assets, replacement pixels and restored parking positions; actual C++ preview: {preview}")


if __name__ == "__main__":
    main()
