#!/usr/bin/env python3
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/effects/missile_trail_flame_smoke_arc_rts_sprite_256.png")


def _arc_points(cx: float, cy: float, radius: float, start_deg: float, end_deg: float, steps: int) -> list[tuple[float, float]]:
    pts: list[tuple[float, float]] = []
    for i in range(steps + 1):
        t = i / steps
        ang = math.radians(start_deg + (end_deg - start_deg) * t)
        pts.append((cx + math.cos(ang) * radius, cy + math.sin(ang) * radius))
    return pts


def draw_missile_trail(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    # Clean parabolic-like arc across the sprite.
    trail = _arc_points(cx=52, cy=218, radius=210, start_deg=-70, end_deg=2, steps=110)

    # Soft smoke body.
    smoke_layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(smoke_layer, "RGBA")
    for idx, p in enumerate(trail[:-8]):
        t = idx / (len(trail) - 1)
        radius = 15 - 8 * t
        alpha = int(120 - 50 * t)
        gray = int(72 + 20 * t)
        sd.ellipse((p[0] - radius, p[1] - radius, p[0] + radius, p[1] + radius), fill=(gray, gray, gray + 4, alpha))
    smoke_layer = smoke_layer.filter(ImageFilter.GaussianBlur(2.2))
    img.alpha_composite(smoke_layer)

    # Bright flame core near the missile head.
    flame = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    fd = ImageDraw.Draw(flame, "RGBA")
    for idx, p in enumerate(trail[-34:]):
        t = idx / 33
        radius = 8 - 5 * t
        alpha = int(210 - 65 * t)
        fd.ellipse((p[0] - radius, p[1] - radius, p[0] + radius, p[1] + radius), fill=(255, int(190 - 70 * t), 40, alpha))
    flame = flame.filter(ImageFilter.GaussianBlur(0.8))
    img.alpha_composite(flame)

    # Hot tip and small missile silhouette.
    md = ImageDraw.Draw(img, "RGBA")
    head_x, head_y = trail[-1]
    tail_x, tail_y = trail[-6]
    vx, vy = head_x - tail_x, head_y - tail_y
    mag = max(1.0, math.hypot(vx, vy))
    nx, ny = vx / mag, vy / mag
    px, py = -ny, nx

    tip = (head_x + nx * 9, head_y + ny * 9)
    rear_l = (head_x - nx * 11 + px * 4, head_y - ny * 11 + py * 4)
    rear_r = (head_x - nx * 11 - px * 4, head_y - ny * 11 - py * 4)
    md.polygon([tip, rear_l, rear_r], fill=(210, 212, 218, 255))
    md.ellipse((head_x - 6, head_y - 6, head_x + 6, head_y + 6), fill=(255, 245, 188, 230))

    # Thin bright streak for readability at small zoom.
    streak = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    td = ImageDraw.Draw(streak, "RGBA")
    td.line(trail[-46:], fill=(255, 224, 140, 120), width=4)
    streak = streak.filter(ImageFilter.GaussianBlur(0.6))
    img.alpha_composite(streak)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_missile_trail(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
