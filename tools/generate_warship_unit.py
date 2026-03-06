#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/warship_unit_rts_sprite_256.png")


def draw_warship(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.55

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 78, cy + 40, cx + 78, cy + 84), fill=(6, 9, 14, 90))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.0))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Long angular warship hull silhouette to distinguish from transport silhouettes.
    hull = [
        (cx, cy - 86),
        (cx + 40, cy - 54),
        (cx + 52, cy + 34),
        (cx + 20, cy + 86),
        (cx - 20, cy + 86),
        (cx - 52, cy + 34),
        (cx - 40, cy - 54),
    ]
    d.polygon(hull, fill=(84, 97, 118, 255))
    deck = [(cx, cy - 76), (cx + 28, cy - 50), (cx + 36, cy + 24), (cx, cy + 62), (cx - 36, cy + 24), (cx - 28, cy - 50)]
    d.polygon(deck, fill=(120, 132, 148, 255))

    # Command bridge
    bridge_top = [(cx, cy - 44), (cx + 16, cy - 34), (cx, cy - 20), (cx - 16, cy - 34)]
    d.polygon(bridge_top, fill=(152, 164, 180, 255))
    d.polygon([(cx - 16, cy - 34), (cx, cy - 20), (cx, cy + 3), (cx - 14, cy - 8)], fill=(106, 117, 136, 255))
    d.polygon([(cx + 16, cy - 34), (cx, cy - 20), (cx, cy + 3), (cx + 14, cy - 8)], fill=(90, 102, 122, 255))

    # Visible cannons: two side turrets + one fore turret.
    for ox, oy, ang in [(-26, -14, -1), (26, -14, 1), (0, 18, 0)]:
        tx = cx + ox
        ty = cy + oy
        d.ellipse((tx - 10, ty - 8, tx + 10, ty + 8), fill=(68, 78, 94, 255))
        d.rectangle((tx - 3, ty - 24, tx + 3, ty - 8), fill=(160, 170, 184, 255))
        if ang < 0:
            d.polygon([(tx - 3, ty - 24), (tx - 16, ty - 33), (tx - 11, ty - 18)], fill=(160, 170, 184, 255))
        elif ang > 0:
            d.polygon([(tx + 3, ty - 24), (tx + 16, ty - 33), (tx + 11, ty - 18)], fill=(160, 170, 184, 255))

    # Missile pods + deck details imply combat purpose.
    d.rectangle((cx - 32, cy + 20, cx - 10, cy + 30), fill=(66, 76, 92, 255))
    d.rectangle((cx + 10, cy + 20, cx + 32, cy + 30), fill=(66, 76, 92, 255))
    d.line([(cx - 9, cy - 66), (cx - 9, cy + 56)], fill=(56, 66, 82, 210), width=2)
    d.line([(cx + 9, cy - 66), (cx + 9, cy + 56)], fill=(56, 66, 82, 210), width=2)

    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon([(cx, cy - 86), (cx + 52, cy + 34), (cx + 20, cy + 86), (cx - 20, cy + 86), (cx - 52, cy + 34)], outline=(15, 16, 22, 190), width=3)
    contour = contour.filter(ImageFilter.GaussianBlur(0.45))
    img.alpha_composite(contour)
    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_warship(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
