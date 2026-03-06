#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/buildings/military_barracks_rts_sprite_256.png")


def draw_barracks(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    cx = size * 0.5
    cy = size * 0.54

    # Ground shadow for separation.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow, "RGBA")
    sdraw.ellipse((cx - 92, cy + 12, cx + 92, cy + 72), fill=(16, 14, 10, 92))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.5))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Stone military compound base.
    base_top = [(cx, cy - 58), (cx + 86, cy - 16), (cx, cy + 34), (cx - 86, cy - 16)]
    base_front = [(cx - 86, cy - 16), (cx, cy + 34), (cx, cy + 56), (cx - 86, cy + 6)]
    base_side = [(cx + 86, cy - 16), (cx, cy + 34), (cx, cy + 56), (cx + 86, cy + 6)]
    d.polygon(base_top, fill=(128, 125, 120, 255))
    d.polygon(base_front, fill=(110, 106, 100, 255))
    d.polygon(base_side, fill=(97, 94, 90, 255))

    # Main barracks hall.
    hall_top = [(cx + 4, cy - 50), (cx + 68, cy - 18), (cx + 4, cy + 16), (cx - 60, cy - 18)]
    hall_front = [(cx - 60, cy - 18), (cx + 4, cy + 16), (cx + 4, cy + 46), (cx - 60, cy + 12)]
    hall_side = [(cx + 68, cy - 18), (cx + 4, cy + 16), (cx + 4, cy + 46), (cx + 68, cy + 12)]
    d.polygon(hall_top, fill=(146, 144, 139, 255))
    d.polygon(hall_front, fill=(124, 120, 114, 255))
    d.polygon(hall_side, fill=(106, 103, 99, 255))

    # Battlements and door.
    for dx in (-44, -28, -12, 4, 20, 36, 52):
        d.rectangle((cx + dx - 4, cy - 35, cx + dx + 4, cy - 26), fill=(88, 85, 82, 255))
    door = [(cx - 12, cy + 2), (cx + 4, cy + 12), (cx + 4, cy + 34), (cx - 12, cy + 24)]
    d.polygon(door, fill=(58, 42, 28, 255))

    # Training yard in front.
    yard = [(cx - 78, cy - 2), (cx - 12, cy + 30), (cx - 44, cy + 46), (cx - 110, cy + 14)]
    d.polygon(yard, fill=(128, 104, 76, 255))

    # Weapon racks in yard.
    rack_posts = [
        (cx - 92, cy + 2),
        (cx - 82, cy + 8),
        (cx - 72, cy + 12),
    ]
    for px, py in rack_posts:
        d.line([(px, py), (px + 2, py + 22)], fill=(62, 44, 28, 255), width=3)
        d.line([(px + 9, py + 4), (px + 11, py + 26)], fill=(62, 44, 28, 255), width=3)
        d.line([(px - 1, py + 8), (px + 10, py + 13)], fill=(78, 56, 35, 255), width=3)
        # Spear heads
        d.polygon([(px + 2, py - 5), (px + 5, py - 1), (px, py - 1)], fill=(170, 170, 176, 255))
        d.polygon([(px + 11, py - 1), (px + 14, py + 3), (px + 9, py + 3)], fill=(170, 170, 176, 255))

    # Team banner on pole (clearly visible).
    pole_top = (cx + 70, cy - 74)
    pole_bottom = (cx + 74, cy - 8)
    d.line([pole_top, pole_bottom], fill=(64, 46, 28, 255), width=4)

    banner = [
        (pole_top[0] + 2, pole_top[1] + 2),
        (pole_top[0] + 36, pole_top[1] + 14),
        (pole_top[0] + 18, pole_top[1] + 30),
        (pole_top[0] + 2, pole_top[1] + 22),
    ]
    d.polygon(banner, fill=(198, 36, 48, 255))
    d.line([(banner[0][0] + 5, banner[0][1] + 5), (banner[1][0] - 6, banner[1][1] + 1)], fill=(244, 210, 86, 230), width=3)
    d.line([(banner[0][0] + 5, banner[0][1] + 12), (banner[1][0] - 6, banner[1][1] + 8)], fill=(244, 210, 86, 230), width=3)

    # Perimeter contour.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [(cx, cy - 62), (cx + 92, cy - 16), (cx + 6, cy + 58), (cx - 112, cy + 16)],
        outline=(24, 20, 16, 165),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.45))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img = draw_barracks(SIZE)
    img.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
