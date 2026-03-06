#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/tank_unit_rts_sprite_256.png")


def draw_tank(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.57

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 82, cy + 24, cx + 82, cy + 72), fill=(10, 10, 10, 92))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.5))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")
    olive = (98, 112, 80, 255)
    olive_dark = (70, 82, 58, 255)
    olive_light = (130, 146, 106, 255)
    team = (40, 116, 220, 255)

    # Heavy tracked hull block.
    d.rounded_rectangle((cx - 70, cy - 34, cx + 70, cy + 42), radius=14, fill=olive)
    d.polygon([(cx - 70, cy - 34), (cx + 70, cy - 34), (cx + 48, cy - 56), (cx - 48, cy - 56)], fill=olive_light)
    d.rounded_rectangle((cx - 82, cy + 24, cx + 82, cy + 50), radius=12, fill=(58, 62, 68, 255))

    # Track rhythm for silhouette readability.
    for i in range(-5, 6):
        x = cx + i * 14
        d.ellipse((x - 7, cy + 29, x + 7, cy + 45), fill=(88, 94, 104, 255))

    # Large turret with oversized cannon.
    turret = [(cx - 42, cy - 18), (cx + 42, cy - 18), (cx + 52, cy + 12), (cx + 26, cy + 34), (cx - 26, cy + 34), (cx - 52, cy + 12)]
    d.polygon(turret, fill=olive_dark)
    d.polygon([(cx - 28, cy - 10), (cx + 28, cy - 10), (cx + 34, cy + 10), (cx - 34, cy + 10)], fill=olive)

    d.rectangle((cx - 8, cy - 98, cx + 8, cy - 8), fill=(136, 146, 122, 255))
    d.polygon([(cx - 8, cy - 98), (cx + 8, cy - 98), (cx + 16, cy - 110), (cx - 16, cy - 110)], fill=(150, 162, 136, 255))
    d.rectangle((cx - 14, cy - 24, cx + 14, cy - 8), fill=(62, 72, 54, 255))

    # Team markings for quick allegiance ID.
    d.rectangle((cx - 58, cy - 20, cx - 34, cy - 6), fill=team)
    d.rectangle((cx + 34, cy - 20, cx + 58, cy - 6), fill=team)
    d.line([(cx - 55, cy - 13), (cx - 37, cy - 13)], fill=(152, 202, 255, 220), width=2)

    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon([(cx - 82, cy + 24), (cx - 70, cy - 34), (cx - 48, cy - 56), (cx, cy - 112), (cx + 48, cy - 56), (cx + 70, cy - 34), (cx + 82, cy + 24), (cx + 82, cy + 50), (cx - 82, cy + 50)], outline=(18, 20, 14, 185), width=3)
    contour = contour.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_tank(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
