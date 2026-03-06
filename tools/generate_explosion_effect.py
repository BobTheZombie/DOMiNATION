#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/effects/explosion_fire_smoke_shockwave_rts_sprite_256.png")


def _radial_gradient(size: int, radius: float, inner: tuple[int, int, int, int], outer: tuple[int, int, int, int]) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    cx = cy = size * 0.5
    for y in range(size):
        for x in range(size):
            dist = math.hypot(x - cx, y - cy)
            t = min(1.0, max(0.0, dist / radius))
            # Ease keeps dense center and soft sprite edges.
            t = t * t * (3.0 - 2.0 * t)
            px[x, y] = tuple(int(inner[i] * (1.0 - t) + outer[i] * t) for i in range(4))
    return img


def draw_explosion(size: int) -> Image.Image:
    random.seed(42)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = cy = size * 0.5

    # Shockwave ring: broad and semi-transparent to read at RTS zoom.
    shock = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shock, "RGBA")
    sd.ellipse((cx - 95, cy - 95, cx + 95, cy + 95), outline=(255, 238, 188, 150), width=7)
    sd.ellipse((cx - 104, cy - 104, cx + 104, cy + 104), outline=(255, 186, 96, 90), width=4)
    shock = shock.filter(ImageFilter.GaussianBlur(1.6))
    img.alpha_composite(shock)

    # Smoke cloud in separate layer for soft blending.
    smoke = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    smd = ImageDraw.Draw(smoke, "RGBA")
    smoke_colors = [(38, 38, 42, 160), (52, 50, 52, 138), (70, 66, 60, 118)]
    for i in range(18):
        ang = (math.tau / 18) * i + random.uniform(-0.1, 0.1)
        dist = random.uniform(38, 74)
        rad = random.uniform(20, 34)
        x = cx + math.cos(ang) * dist
        y = cy + math.sin(ang) * dist
        col = smoke_colors[i % len(smoke_colors)]
        smd.ellipse((x - rad, y - rad, x + rad, y + rad), fill=col)
    smd.ellipse((cx - 48, cy - 48, cx + 48, cy + 48), fill=(58, 52, 44, 154))
    smoke = smoke.filter(ImageFilter.GaussianBlur(5.2))
    img.alpha_composite(smoke)

    # Fire burst core.
    core = _radial_gradient(size, 54, (255, 255, 224, 235), (255, 168, 46, 0))
    img.alpha_composite(core)

    # Irregular flame petals to make the blast feel energetic.
    flames = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    fd = ImageDraw.Draw(flames, "RGBA")
    for i in range(14):
        ang = (math.tau / 14) * i + random.uniform(-0.14, 0.14)
        reach = random.uniform(52, 88)
        spread = random.uniform(0.14, 0.23)
        inner = 26
        p1 = (cx + math.cos(ang - spread) * inner, cy + math.sin(ang - spread) * inner)
        p2 = (cx + math.cos(ang) * reach, cy + math.sin(ang) * reach)
        p3 = (cx + math.cos(ang + spread) * inner, cy + math.sin(ang + spread) * inner)
        warm = random.randint(190, 235)
        fd.polygon([p1, p2, p3], fill=(255, warm, 40, 170))
    flames = flames.filter(ImageFilter.GaussianBlur(2.4))
    img.alpha_composite(flames)

    # Hot center + embers.
    center = _radial_gradient(size, 30, (255, 255, 246, 255), (255, 150, 42, 0))
    img.alpha_composite(center)

    embers = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ed = ImageDraw.Draw(embers, "RGBA")
    for _ in range(26):
        ang = random.uniform(0.0, math.tau)
        dist = random.uniform(30, 102)
        r = random.uniform(1.0, 3.0)
        x = cx + math.cos(ang) * dist
        y = cy + math.sin(ang) * dist
        ed.ellipse((x - r, y - r, x + r, y + r), fill=(255, random.randint(168, 236), 90, random.randint(110, 190)))
    embers = embers.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(embers)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_explosion(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
