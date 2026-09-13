"""Validate the actual sprite atlas and execute the C++ renderer on the host.

Run with Python, Pillow, NumPy, and an MSVC or g++ compiler. All binary build
products are kept outside the ESPHome source tree.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "ants_v2"
spec = importlib.util.spec_from_file_location("sprite_builder", ROOT / "tools" / "build_ants_v2_sprites.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def rgb565(rgb):
    rgb = rgb.astype(np.uint16)
    return ((rgb[..., 0] >> 3) << 11) | ((rgb[..., 1] >> 2) << 5) | (rgb[..., 2] >> 3)


def blend_over_background(rgba, opacity):
    values = rgb565(rgba[:, :, :3]).astype(np.uint32)
    r5, g6, b5 = values >> 11, (values >> 5) & 63, values & 31
    src = np.stack(((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)), axis=-1)
    alpha = (rgba[:, :, 3].astype(np.uint32) * opacity + 127) // 255
    # The existing framebuffer stores RGB565, whose expanded background is 0,8,8.
    result = (src * alpha[:, :, None] + np.array([0, 8, 8]) * (255 - alpha[:, :, None]) + 127) // 255
    return rgb565(result.astype(np.uint8))


def validate_assets(manifest):
    assert manifest["frames"] == 12 and manifest["directions"] == 64
    assert len(manifest["banks"]) == 3
    for filename, digest in manifest["source_sha256"].items():
        assert hashlib.sha256((ASSETS / "source" / filename).read_bytes()).hexdigest() == digest
    for filename, digest in manifest["atlas_sha256"].items():
        assert hashlib.sha256((ASSETS / filename).read_bytes()).hexdigest() == digest
    for bank in manifest["banks"]:
        image = Image.open(ASSETS / f'atlas_{bank["name"]}.png')
        assert image.mode == "RGBA" and image.size == (bank["width"], bank["height"])
        occupancy = np.zeros((image.height, image.width), np.uint8)
        assert len(bank["sprites"]) == 12 * 64
        for x, y, w, h, dx, dy in bank["sprites"]:
            assert 0 < w <= 32 and 0 < h <= 32
            assert 0 <= x and x + w <= image.width and 0 <= y and y + h <= image.height
            assert -32 <= dx <= 32 and -32 <= dy <= 32
            region = occupancy[y:y+h, x:x+w]
            assert not region.any(), "Overlapping sprite rectangles"
            region[:] = 1
            alpha = image.crop((x, y, x+w, y+h)).getchannel("A")
            assert alpha.getextrema()[1] > 0
    total = sum(bank["compiled_bytes"] for bank in manifest["banks"])
    assert total < 4 * 1024 * 1024, "Atlas exceeded the chosen flash budget"

    frames = [Image.open(ASSETS / "walk" / f"frame_{i:02d}.png") for i in range(12)]
    assert len({hashlib.sha256(frame.tobytes()).hexdigest() for frame in frames}) == 12
    body = builder.rgba(ASSETS / "source" / "body.png")
    bounds = body.getchannel("A").point(lambda v: 255 if v >= 8 else 0).getbbox()
    body = body.crop((bounds[0]-4, bounds[1]-4, bounds[2]+4, bounds[3]+4))
    height = round(17 * builder.S)
    width = round(height * body.width / body.height)
    body = body.resize((width, height), Image.Resampling.LANCZOS)
    mask = np.array(body)[:, :, 3] == 255
    x, y = round(256-width/2), round(8*builder.S)
    reference = np.array(frames[0])[y:y+height, x:x+width]
    for frame in frames[1:]:
        assert np.array_equal(np.array(frame)[y:y+height, x:x+width][mask], reference[mask]), "Body wobbles between frames"
    print(f"PASS: 2,304 non-overlapping sprites, true alpha, 12 distinct frames, fixed body; {total:,} compiled atlas bytes.")


def compile_host(directory, source=None):
    source = source or ROOT / "tests" / "ants_v2_host.cpp"
    executable = directory / (source.stem + (".exe" if os.name == "nt" else ""))
    include = [ROOT / "tests" / "host_stubs", ROOT]
    if os.name == "nt":
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
        vcvars = Path(installation) / "VC/Auxiliary/Build/vcvars64.bat"
        setup = directory / "host-compiler-env.cmd"
        setup.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\nset\n')
        result = subprocess.run(["cmd", "/d", "/c", str(setup)], capture_output=True, text=True, check=True)
        # Environment values are used in memory only and never logged or stored.
        env = {key.upper(): value for key, value in os.environ.items()}
        for line in result.stdout.splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                if key:
                    env[key.upper()] = value
        compiler = shutil.which("cl.exe", path=env["PATH"])
        command = [compiler, "/nologo", "/std:c++17", "/EHsc", "/O2", "/utf-8", "/D_CRT_SECURE_NO_WARNINGS",
                   *[f"/I{p}" for p in include], str(source), f"/Fe:{executable}", f"/Fo:{directory / 'host.obj'}"]
    else:
        env = os.environ.copy()
        command = ["g++", "-std=c++17", "-O2", *[f"-I{p}" for p in include], str(source), "-o", str(executable)]
    subprocess.run(command, env=env, cwd=directory, check=True)
    return executable


def prepare_binary_inputs(directory, manifest):
    for bank in manifest["banks"]:
        data = np.array(Image.open(ASSETS / f'atlas_{bank["name"]}.png'))
        colors = rgb565(data[:, :, :3])
        planar = colors.astype("<u2").tobytes() + data[:, :, 3].tobytes()
        (directory / f'{bank["name"]}.bin').write_bytes(planar)
        if bank["name"] == "gold":
            legacy = np.stack((colors >> 8, colors & 255, data[:, :, 3]), axis=-1).astype(np.uint8)
            (directory / "gold_legacy.bin").write_bytes(legacy.tobytes())


def verify_rendered_pixels(directory):
    cases = [(100,100,0,0,0,255), (100,100,3,16,0,255), (100,100,6,32,0,255),
             (100,100,9,48,0,255), (100,100,5,7,1,255), (100,100,11,63,2,214),
             (0,0,4,13,0,255), (479,319,8,25,2,182)]
    for index, (x, y, frame, direction, bank, opacity) in enumerate(cases):
        # Independent reference: draw the full untrimmed sprite in logical screen
        # coordinates, then compare against the actual C++ native framebuffer.
        source = Image.open(ASSETS / "walk" / f"frame_{frame:02d}.png")
        name, scale = builder.BANKS[bank]
        source = builder.color_variant(source, name)
        source = source.rotate(-360 * direction / 64, Image.Resampling.BICUBIC, expand=False)
        size = round(32 * scale)
        source = source.resize((size, size), Image.Resampling.LANCZOS)
        pixels = blend_over_background(np.array(source), opacity)
        expected = np.full((320, 480), (2 << 5) | 1, np.uint16)
        left, top = x - size//2, y - size//2
        x0, y0, x1, y1 = max(0, left), max(0, top), min(480, left+size), min(320, top+size)
        expected[y0:y1, x0:x1] = pixels[y0-top:y1-top, x0-left:x1-left]
        raw = np.frombuffer((directory / f"case_{index}.bin").read_bytes(), dtype=">u2").reshape(480, 320)
        actual = np.rot90(raw)
        if not np.array_equal(actual, expected):
            mismatch = np.count_nonzero(actual != expected)
            raise AssertionError(f"C++ pixel/rotation mismatch in case {index}: {mismatch} pixels")
    raw = np.frombuffer((directory / "clock.bin").read_bytes(), dtype=">u2").reshape(480, 320)
    logical = np.rot90(raw).astype(np.uint32)
    r, g, b = logical >> 11, (logical >> 5) & 63, logical & 31
    rgb = np.stack(((r*255+15)//31, (g*255+31)//63, (b*255+15)//31), axis=-1).astype(np.uint8)
    Image.fromarray(rgb).save(ASSETS / "preview" / "clock.png")
    print("PASS: native orientation and alpha blending match independent logical reference pixel-for-pixel.")


def export_animation(directory):
    frames = []
    for index in range(200):
        raw = np.frombuffer((directory / f"preview_{index}.bin").read_bytes(), dtype=">u2").reshape(480, 320)
        pixels = np.rot90(raw).astype(np.uint32)
        rgb = np.stack((((pixels >> 11) * 255 + 15) // 31,
                        (((pixels >> 5) & 63) * 255 + 31) // 63,
                        ((pixels & 31) * 255 + 15) // 31), axis=-1).astype(np.uint8)
        frames.append(Image.fromarray(rgb))
    # A shared palette avoids flicker. Alternating 80/90 ms GIF delays retain
    # the average 84 ms cadence of every second simulated firmware frame.
    palette_sheet = Image.new("RGB", (480, 320 * 20))
    for row, frame in enumerate(frames[::10]):
        palette_sheet.paste(frame, (0, row * 320))
    palette = palette_sheet.quantize(colors=256)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    destination = ASSETS / "preview" / "clock-interaction.gif"
    indexed[0].save(destination, save_all=True, append_images=indexed[1:],
                    duration=[80, 80, 90, 80, 90] * 40, loop=0, optimize=False)
    print(f"Actual C++ minute-change, tap and drag animation: {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--preview", action="store_true", help="Export an animated clock and touch demo")
    args = parser.parse_args()
    directory = args.build_dir or Path(tempfile.mkdtemp(prefix="ants-v2-test-"))
    directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    validate_assets(manifest)
    prepare_binary_inputs(directory, manifest)
    executable = compile_host(directory)
    subprocess.run([str(executable), str(directory)] + (["--preview"] if args.preview else []), check=True)
    verify_rendered_pixels(directory)
    if args.preview:
        export_animation(directory)
    print("Host tests passed. Device frame rate still requires hardware measurement.")


if __name__ == "__main__":
    main()
