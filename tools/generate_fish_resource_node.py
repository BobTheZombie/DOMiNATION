#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 256
OUT_PATH = Path("content/textures/resources/fish_resource_node_rts_sprite_256.png")
SEED = 982451


def clamp_u8(value: float) -> int:
    return int(max(0, min(255, round(value))))


def fish_points(cx: float, cy: float, length: float, width: float, heading: float) -> list[tuple[float, float]]:
    c = math.cos(heading)
    s = math.sin(heading)

    def rot(px: float, py: float) -> tuple[float, float]:
        return (cx + px * c - py * s, cy + px * s + py * c)

    body_front = length * 0.52
    body_back = -length * 0.34
    tail_tip = -length * 0.66
    fin = width * 0.58

    base_shape = [
        (body_front, 0.0),
        (length * 0.20, width * 0.42),
        (body_back, width * 0.34),
        (tail_tip, fin),
        (body_back + length * 0.05, 0.0),
        (tail_tip, -fin),
        (body_back, -width * 0.34),
        (length * 0.20, -width * 0.42),
    ]
    return [rot(px, py) for px, py in base_shape]


def draw_fish_school(draw: ImageDraw.ImageDraw, rng: random.Random, center: tuple[float, float]) -> None:
    cx, cy = center
    fish_count = 7
    spread = 36.0

    fish_dark = (38, 86, 118, 220)
    fish_light = (68, 132, 166, 218)

    for i in range(fish_count):
        orbit = i / fish_count * math.tau + rng.uniform(-0.18, 0.18)
        radius = rng.uniform(spread * 0.20, spread)
        fx = cx + math.cos(orbit) * radius
        fy = cy + math.sin(orbit) * radius

        heading = orbit + math.pi / 2.0 + rng.uniform(-0.5, 0.5)
        length = rng.uniform(18.0, 22.0)
        width = rng.uniform(8.0, 10.0)

        tint = rng.uniform(0.0, 1.0)
        color = (
            clamp_u8(fish_dark[0] + (fish_light[0] - fish_dark[0]) * tint),
            clamp_u8(fish_dark[1] + (fish_light[1] - fish_dark[1]) * tint),
            clamp_u8(fish_dark[2] + (fish_light[2] - fish_dark[2]) * tint),
            clamp_u8(fish_dark[3] + (fish_light[3] - fish_dark[3]) * tint),
        )

        draw.polygon(fish_points(fx, fy, length, width, heading), fill=color)


def draw_ripples(draw: ImageDraw.ImageDraw, center: tuple[float, float]) -> None:
    cx, cy = center
    ring_specs = [
        (68, 40, 1.5),
        (86, 30, 1.3),
        (104, 22, 1.1),
    ]

    for diameter, alpha, stroke in ring_specs:
        bbox = [cx - diameter / 2.0, cy - diameter / 2.0, cx + diameter / 2.0, cy + diameter / 2.0]
        draw.ellipse(bbox, outline=(188, 231, 255, alpha), width=math.ceil(stroke))

    # Broken arc highlight to keep shape readable from strategic zoom.
    highlight_bbox = [cx - 55, cy - 55, cx + 55, cy + 55]
    draw.arc(highlight_bbox, start=210, end=332, fill=(218, 244, 255, 72), width=2)


def make_sprite(size: int, seed: int) -> Image.Image:
    rng = random.Random(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img, "RGBA")

    center = (size * 0.5, size * 0.52)

    draw_ripples(draw, center)
    draw_fish_school(draw, rng, center)

    # Soft caustic tint under school to ground it in water without filling the background.
    caustic = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    caustic_draw = ImageDraw.Draw(caustic, "RGBA")
    caustic_draw.ellipse(
        [center[0] - 48, center[1] - 38, center[0] + 48, center[1] + 38],
        fill=(116, 192, 224, 26),
    )
    img.alpha_composite(caustic)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = make_sprite(SIZE, SEED)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
