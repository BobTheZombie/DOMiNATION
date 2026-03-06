#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/infantry_unit_rts_sprite_256.png")


def draw_infantry(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    cx = size * 0.5
    cy = size * 0.54

    # Soft ground shadow for legibility over noisy terrain.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 62, cy + 22, cx + 62, cy + 86), fill=(8, 8, 10, 105))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.0))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    steel_light = (166, 176, 193, 255)
    steel_mid = (126, 137, 154, 255)
    steel_dark = (90, 99, 116, 255)
    leather = (99, 70, 44, 255)
    team = (192, 46, 58, 255)
    team_light = (232, 120, 130, 255)

    # Shield first: large and offset for a strong silhouette at small size.
    shield = [
        (cx - 88, cy - 44),
        (cx - 124, cy - 18),
        (cx - 112, cy + 38),
        (cx - 72, cy + 64),
        (cx - 42, cy + 40),
        (cx - 50, cy - 20),
    ]
    d.polygon(shield, fill=(112, 80, 56, 255))
    d.polygon(
        [(cx - 82, cy - 32), (cx - 110, cy - 12), (cx - 100, cy + 28), (cx - 74, cy + 45), (cx - 53, cy + 29), (cx - 58, cy - 14)],
        fill=(132, 96, 66, 255),
    )
    d.polygon(
        [(cx - 74, cy - 22), (cx - 96, cy - 6), (cx - 89, cy + 22), (cx - 70, cy + 34), (cx - 55, cy + 22), (cx - 59, cy - 11)],
        fill=team,
    )
    d.ellipse((cx - 83, cy + 0, cx - 67, cy + 16), fill=(210, 176, 110, 255))

    # Helmeted head.
    d.ellipse((cx - 16, cy - 82, cx + 16, cy - 52), fill=steel_light)
    d.polygon([(cx - 20, cy - 71), (cx + 20, cy - 71), (cx + 14, cy - 58), (cx - 14, cy - 58)], fill=steel_mid)
    d.line([(cx - 1, cy - 80), (cx - 1, cy - 53)], fill=steel_dark, width=3)

    # Chest armor as chunky geometric plates.
    torso_top = [(cx, cy - 54), (cx + 44, cy - 32), (cx, cy - 6), (cx - 44, cy - 32)]
    torso_front = [(cx - 44, cy - 32), (cx, cy - 6), (cx, cy + 36), (cx - 40, cy + 12)]
    torso_side = [(cx + 44, cy - 32), (cx, cy - 6), (cx, cy + 36), (cx + 40, cy + 12)]
    d.polygon(torso_top, fill=steel_light)
    d.polygon(torso_front, fill=steel_mid)
    d.polygon(torso_side, fill=steel_dark)

    # Team color tabard strip on chest/back.
    d.polygon([(cx - 10, cy - 28), (cx + 10, cy - 18), (cx + 10, cy + 28), (cx - 10, cy + 16)], fill=team)
    d.line([(cx - 8, cy - 20), (cx + 8, cy - 12)], fill=team_light, width=2)

    # Legs in wide stance.
    d.polygon([(cx - 24, cy + 8), (cx - 2, cy + 20), (cx - 16, cy + 64), (cx - 38, cy + 52)], fill=steel_dark)
    d.polygon([(cx + 2, cy + 20), (cx + 24, cy + 8), (cx + 38, cy + 52), (cx + 16, cy + 64)], fill=steel_mid)
    d.ellipse((cx - 44, cy + 50, cx - 14, cy + 72), fill=(52, 42, 34, 255))
    d.ellipse((cx + 14, cy + 50, cx + 44, cy + 72), fill=(52, 42, 34, 255))

    # Arms with visible pauldrons.
    d.polygon([(cx - 46, cy - 22), (cx - 28, cy - 14), (cx - 42, cy + 12), (cx - 60, cy + 4)], fill=steel_mid)
    d.polygon([(cx + 28, cy - 14), (cx + 46, cy - 22), (cx + 60, cy + 4), (cx + 42, cy + 12)], fill=steel_dark)

    # Sword on right side: long diagonal blade for immediate role recognition.
    d.line([(cx + 58, cy - 2), (cx + 108, cy - 76)], fill=(196, 206, 222, 255), width=7)
    d.line([(cx + 60, cy - 1), (cx + 105, cy - 70)], fill=(150, 162, 180, 210), width=2)
    d.rectangle((cx + 46, cy - 8, cx + 69, cy + 0), fill=(208, 172, 110, 255))
    d.line([(cx + 58, cy + 2), (cx + 58, cy + 16)], fill=leather, width=6)

    # Back banner ribbon for second team-color hit.
    d.polygon([(cx + 24, cy - 42), (cx + 46, cy - 32), (cx + 44, cy - 8), (cx + 24, cy - 16)], fill=team)

    # Outline pass preserves silhouette on light backgrounds.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [
            (cx - 124, cy - 18),
            (cx - 112, cy + 38),
            (cx - 76, cy + 67),
            (cx - 40, cy + 58),
            (cx + 0, cy + 72),
            (cx + 40, cy + 58),
            (cx + 66, cy + 10),
            (cx + 110, cy - 78),
            (cx + 20, cy - 86),
            (cx - 22, cy - 86),
            (cx - 90, cy - 48),
        ],
        outline=(16, 14, 14, 180),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_infantry(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
