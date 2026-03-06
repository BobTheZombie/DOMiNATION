#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/archer_unit_rts_sprite_256.png")


def draw_archer(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    cx = size * 0.5
    cy = size * 0.54

    # Grounded shadow keeps the silhouette readable on mixed RTS terrain.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 56, cy + 30, cx + 56, cy + 84), fill=(10, 10, 9, 90))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.0))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Torso with light armor layers.
    chest_top = [(cx, cy - 56), (cx + 40, cy - 34), (cx, cy - 10), (cx - 40, cy - 34)]
    chest_front = [(cx - 40, cy - 34), (cx, cy - 10), (cx, cy + 36), (cx - 40, cy + 12)]
    chest_side = [(cx + 40, cy - 34), (cx, cy - 10), (cx + 40, cy + 12), (cx, cy + 36)]
    d.polygon(chest_top, fill=(138, 148, 160, 255))
    d.polygon(chest_front, fill=(106, 114, 126, 255))
    d.polygon(chest_side, fill=(94, 102, 114, 255))

    # Team accent sash keeps faction readability.
    sash = [(cx - 18, cy - 28), (cx + 14, cy - 12), (cx - 8, cy + 4), (cx - 40, cy - 12)]
    d.polygon(sash, fill=(40, 118, 224, 245))

    # Head and leather cap.
    d.ellipse((cx - 18, cy - 82, cx + 18, cy - 46), fill=(208, 169, 132, 255))
    cap = [(cx - 21, cy - 78), (cx + 21, cy - 78), (cx + 12, cy - 63), (cx - 12, cy - 63)]
    d.polygon(cap, fill=(98, 73, 46, 255))

    # Arms posed to make bow shape clear.
    back_arm = [(cx - 34, cy - 16), (cx - 14, cy - 6), (cx - 24, cy + 24), (cx - 44, cy + 14)]
    bow_arm = [(cx + 16, cy - 12), (cx + 36, cy - 20), (cx + 48, cy + 8), (cx + 28, cy + 18)]
    d.polygon(back_arm, fill=(208, 169, 132, 255))
    d.polygon(bow_arm, fill=(208, 169, 132, 255))

    # Legs and boots, wider stance for small-size readability.
    left_leg = [(cx - 20, cy + 12), (cx - 2, cy + 24), (cx - 12, cy + 64), (cx - 30, cy + 52)]
    right_leg = [(cx + 2, cy + 24), (cx + 20, cy + 12), (cx + 30, cy + 52), (cx + 12, cy + 64)]
    d.polygon(left_leg, fill=(72, 78, 90, 255))
    d.polygon(right_leg, fill=(76, 82, 94, 255))
    d.ellipse((cx - 36, cy + 50, cx - 9, cy + 71), fill=(50, 37, 25, 255))
    d.ellipse((cx + 9, cy + 50, cx + 36, cy + 71), fill=(50, 37, 25, 255))

    # Quiver at the back with visible arrow fletching.
    quiver = [(cx - 50, cy - 54), (cx - 34, cy - 46), (cx - 48, cy + 4), (cx - 64, cy - 4)]
    d.polygon(quiver, fill=(100, 67, 39, 255))
    d.line([(cx - 58, cy - 60), (cx - 42, cy - 52)], fill=(188, 196, 204, 255), width=3)
    d.polygon(
        [(cx - 64, cy - 70), (cx - 58, cy - 66), (cx - 62, cy - 58), (cx - 68, cy - 62)],
        fill=(182, 48, 46, 255),
    )
    d.polygon(
        [(cx - 54, cy - 74), (cx - 48, cy - 70), (cx - 52, cy - 62), (cx - 58, cy - 66)],
        fill=(210, 218, 226, 255),
    )

    # Bow is intentionally oversized and offset for clear RTS readability.
    bow_points = [
        (cx + 46, cy - 84),
        (cx + 66, cy - 60),
        (cx + 72, cy - 26),
        (cx + 68, cy + 14),
        (cx + 56, cy + 46),
        (cx + 44, cy + 64),
    ]
    d.line(bow_points, fill=(116, 78, 42, 255), width=7, joint="curve")
    d.line([(cx + 46, cy - 82), (cx + 44, cy + 62)], fill=(224, 224, 214, 220), width=2)

    # Drawn arrow crossing the torso to reinforce unit role.
    d.line([(cx - 2, cy - 6), (cx + 48, cy - 28)], fill=(143, 103, 62, 255), width=4)
    d.polygon([(cx + 52, cy - 30), (cx + 44, cy - 32), (cx + 46, cy - 24)], fill=(188, 194, 204, 255))
    d.polygon([(cx - 8, cy - 2), (cx - 14, cy + 4), (cx - 2, cy + 2)], fill=(198, 54, 52, 255))

    # Light shoulder plates for armor readability.
    d.polygon([(cx - 28, cy - 38), (cx - 10, cy - 28), (cx - 20, cy - 16), (cx - 38, cy - 26)], fill=(152, 161, 171, 255))
    d.polygon([(cx + 10, cy - 28), (cx + 28, cy - 38), (cx + 38, cy - 26), (cx + 20, cy - 16)], fill=(152, 161, 171, 255))

    # Dark contour keeps sprite readable on bright backgrounds.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [
            (cx - 70, cy - 68),
            (cx - 64, cy + 8),
            (cx - 30, cy + 66),
            (cx + 38, cy + 66),
            (cx + 78, cy + 14),
            (cx + 76, cy - 30),
            (cx + 66, cy - 62),
            (cx + 44, cy - 88),
            (cx + 8, cy - 84),
            (cx - 18, cy - 84),
        ],
        outline=(18, 15, 12, 170),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.45))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_archer(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
