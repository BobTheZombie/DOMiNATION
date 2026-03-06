#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/strategic_bomber_unit_rts_sprite_256.png")


def draw_bomber(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.54

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 92, cy + 34, cx + 92, cy + 72), fill=(6, 8, 10, 84))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.2))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")
    body = (108, 118, 132, 255)
    body_light = (144, 154, 170, 255)
    body_dark = (84, 94, 108, 255)

    # Central fuselage.
    fuselage = [(cx, cy - 92), (cx + 22, cy - 54), (cx + 18, cy + 56), (cx, cy + 88), (cx - 18, cy + 56), (cx - 22, cy - 54)]
    d.polygon(fuselage, fill=body)
    d.polygon([(cx, cy - 78), (cx + 12, cy - 48), (cx + 10, cy + 44), (cx, cy + 72), (cx - 10, cy + 44), (cx - 12, cy - 48)], fill=body_light)

    # Wide strategic wings for top-down readability.
    left_wing = [(cx - 20, cy - 20), (cx - 118, cy + 18), (cx - 104, cy + 46), (cx - 24, cy + 24)]
    right_wing = [(cx + 20, cy - 20), (cx + 118, cy + 18), (cx + 104, cy + 46), (cx + 24, cy + 24)]
    d.polygon(left_wing, fill=body_dark)
    d.polygon(right_wing, fill=body_dark)
    d.polygon([(cx - 18, cy - 18), (cx - 98, cy + 12), (cx - 88, cy + 30), (cx - 20, cy + 16)], fill=body)
    d.polygon([(cx + 18, cy - 18), (cx + 98, cy + 12), (cx + 88, cy + 30), (cx + 20, cy + 16)], fill=body)

    # Tail and horizontal stabilizers.
    d.polygon([(cx, cy + 52), (cx + 18, cy + 74), (cx, cy + 96), (cx - 18, cy + 74)], fill=body_dark)
    d.polygon([(cx - 14, cy + 56), (cx - 62, cy + 72), (cx - 52, cy + 88), (cx - 6, cy + 72)], fill=body)
    d.polygon([(cx + 14, cy + 56), (cx + 62, cy + 72), (cx + 52, cy + 88), (cx + 6, cy + 72)], fill=body)

    # Bomb payload bay and bombs make role explicit.
    d.rounded_rectangle((cx - 12, cy - 8, cx + 12, cy + 38), radius=5, fill=(56, 64, 74, 255))
    for by in [4, 18, 32]:
        d.ellipse((cx - 22, cy + by, cx - 14, cy + by + 12), fill=(72, 80, 90, 255))
        d.ellipse((cx + 14, cy + by, cx + 22, cy + by + 12), fill=(72, 80, 90, 255))

    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon([(cx, cy - 92), (cx + 118, cy + 18), (cx + 62, cy + 72), (cx, cy + 96), (cx - 62, cy + 72), (cx - 118, cy + 18)], outline=(16, 18, 22, 178), width=3)
    contour = contour.filter(ImageFilter.GaussianBlur(0.48))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_bomber(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
