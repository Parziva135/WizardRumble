"""Generate original, tile-safe soft-light masks for the Canvas renderer.

RGB and alpha both fall to zero at the border. Thus an additive Canvas path
that does not weight texture alpha still cannot draw a rectangular backdrop.
No external images, blur passes or movie assets are used.
"""
from pathlib import Path
import math
from PIL import Image

root = Path(__file__).resolve().parent.parent
folder = root / "assets" / "textures"
folder.mkdir(parents=True, exist_ok=True)
size = 128

for mist in (False, True):
    image = Image.new("RGBA", (size, size))
    pixels = []
    for y in range(size):
        for x in range(size):
            u = (x + .5 - size / 2) / (size / 2 - 1)
            v = (y + .5 - size / 2) / (size / 2 - 1)
            radius2 = u * u + v * v
            edge = max(0., 1. - radius2)
            mask = math.exp(-2.5 * radius2) * edge * edge
            if mist:
                # Smooth, deterministic cloud density. Soft borders remain
                # perfectly black; no thresholded hard-edged noise islands.
                cloud = (.64 + .17 * math.sin(u * 7.1 + v * 3.8)
                         + .12 * math.cos(v * 8.2 - u * 4.3)
                         + .07 * math.sin(u * 13.7 - v * 10.2))
                mask *= max(.08, cloud)
            value = round(255 * math.sqrt(max(0., min(1., mask))))
            if x in (0, size - 1) or y in (0, size - 1):
                value = 0
            pixels.append((value, value, value, value))
    image.putdata(pixels)
    name = "glow_mist.png" if mist else "glow_soft.png"
    image.save(folder / name, optimize=True)
    assert all(image.getpixel((x, y)) == (0, 0, 0, 0)
               for x in range(size) for y in (0, size - 1))
    assert all(image.getpixel((x, y)) == (0, 0, 0, 0)
               for y in range(size) for x in (0, size - 1))
    assert max(p[0] for p in pixels) >= (220 if mist else 250)
    print(name, image.size, "RGBA; black/transparent borders verified")
