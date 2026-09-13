"""Size the approved transparent crushed-car cutout for both clock renderers."""
import hashlib
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/cars/source/car_crushed_cutout.png"


def main():
    cutout = Image.open(SOURCE).convert("RGBA")
    # Ignore almost transparent extraction specks when finding the car bounds.
    bounds = cutout.getchannel("A").point(lambda a: 255 if a >= 32 else 0).getbbox()
    car = cutout.crop(bounds)
    # Match the existing 28x52 sprite canvas, with the body 48 pixels long.
    height = 48
    width = round(car.width * height / car.height)
    car = car.resize((width, height), Image.Resampling.LANCZOS)
    sprite = Image.new("RGBA", (28, 52))
    sprite.alpha_composite(car, ((28 - width) // 2, 2))
    destinations = [ROOT / "assets/car_crushed.png", ROOT.parent / "pc/assets/car_crushed.png"]
    for destination in destinations:
        sprite.save(destination, optimize=True)
    manifest = {
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "sprite_sha256": hashlib.sha256(destinations[0].read_bytes()).hexdigest(),
        "canvas": [28, 52], "car_size": [width, height], "source_bounds": bounds,
        "compiled_rgba_bytes": 28 * 52 * 4,
        "source_preparation": "Built-in image tool: remove only the exterior black background from the supplied damaged yellow car; preserve car, dents, dark windows and front-up orientation; true transparent PNG, no added objects.",
    }
    (ROOT / "assets/cars/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Saved matching 28x52 RGBA sprites to {destinations}")


if __name__ == "__main__":
    main()
