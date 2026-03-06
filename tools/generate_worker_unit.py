#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/worker_unit_rts_sprite_256.png")


def draw_worker(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    cx = size * 0.5
    cy = size * 0.54

    # Ground shadow keeps the small unit readable on mixed terrain.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 54, cy + 26, cx + 54, cy + 84), fill=(10, 9, 8, 95))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.0))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Top-down body block with simple clothing silhouette.
    torso_top = [(cx, cy - 56), (cx + 42, cy - 35), (cx, cy - 10), (cx - 42, cy - 35)]
    torso_front = [(cx - 42, cy - 35), (cx, cy - 10), (cx, cy + 34), (cx - 42, cy + 9)]
    torso_side = [(cx + 42, cy - 35), (cx, cy - 10), (cx, cy + 34), (cx + 42, cy + 9)]
    d.polygon(torso_top, fill=(124, 98, 76, 255))
    d.polygon(torso_front, fill=(103, 80, 62, 255))
    d.polygon(torso_side, fill=(89, 69, 54, 255))

    # Team-color shoulder sash accent.
    sash = [(cx - 22, cy - 30), (cx + 6, cy - 16), (cx - 20, cy + 1), (cx - 48, cy - 13)]
    d.polygon(sash, fill=(40, 118, 224, 255))
    d.line([(cx - 18, cy - 24), (cx - 3, cy - 17)], fill=(146, 197, 255, 210), width=3)

    # Head and cap (simple peasant worker clothing).
    d.ellipse((cx - 18, cy - 79, cx + 18, cy - 45), fill=(206, 168, 132, 255))
    cap = [(cx - 22, cy - 74), (cx + 22, cy - 74), (cx + 14, cy - 60), (cx - 14, cy - 60)]
    d.polygon(cap, fill=(110, 84, 58, 255))

    # Legs and boots (wider stance for clearer silhouette).
    left_leg = [(cx - 22, cy + 10), (cx - 2, cy + 22), (cx - 12, cy + 62), (cx - 32, cy + 50)]
    right_leg = [(cx + 2, cy + 22), (cx + 22, cy + 10), (cx + 32, cy + 50), (cx + 12, cy + 62)]
    d.polygon(left_leg, fill=(72, 76, 88, 255))
    d.polygon(right_leg, fill=(80, 84, 96, 255))
    d.ellipse((cx - 37, cy + 48, cx - 8, cy + 70), fill=(52, 38, 26, 255))
    d.ellipse((cx + 8, cy + 48, cx + 37, cy + 70), fill=(52, 38, 26, 255))

    # Arms extended with tools to communicate worker role clearly at RTS zoom.
    left_arm = [(cx - 44, cy - 20), (cx - 22, cy - 10), (cx - 37, cy + 18), (cx - 57, cy + 8)]
    right_arm = [(cx + 22, cy - 10), (cx + 44, cy - 20), (cx + 57, cy + 8), (cx + 37, cy + 18)]
    d.polygon(left_arm, fill=(206, 168, 132, 255))
    d.polygon(right_arm, fill=(206, 168, 132, 255))

    # Shovel (left side): wood handle + steel spade.
    d.line([(cx - 58, cy + 0), (cx - 92, cy - 62)], fill=(118, 82, 48, 255), width=6)
    shovel_head = [(cx - 95, cy - 69), (cx - 84, cy - 63), (cx - 90, cy - 50), (cx - 101, cy - 56)]
    d.polygon(shovel_head, fill=(146, 154, 164, 255))

    # Hammer (right side): handle + blocky head.
    d.line([(cx + 58, cy - 2), (cx + 90, cy - 54)], fill=(118, 82, 48, 255), width=6)
    hammer_head = [(cx + 80, cy - 69), (cx + 103, cy - 58), (cx + 98, cy - 47), (cx + 75, cy - 58)]
    d.polygon(hammer_head, fill=(122, 128, 136, 255))
    d.rectangle((cx + 92, cy - 61, cx + 103, cy - 51), fill=(104, 110, 120, 255))

    # Team-color cloth patch on back for identification.
    patch = [(cx + 8, cy - 24), (cx + 24, cy - 16), (cx + 10, cy - 6), (cx - 6, cy -14)]
    d.polygon(patch, fill=(40, 118, 224, 245))

    # Crisp dark contour to preserve silhouette on bright terrain.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [
            (cx - 103, cy - 58),
            (cx - 62, cy + 24),
            (cx - 34, cy + 64),
            (cx + 34, cy + 64),
            (cx + 62, cy + 24),
            (cx + 103, cy - 58),
            (cx + 20, cy - 82),
            (cx - 20, cy - 82),
        ],
        outline=(18, 15, 12, 170),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.45))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_worker(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
