#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/drone_unit_rts_sprite_256.png")


def draw_drone(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.54

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 56, cy + 30, cx + 56, cy + 64), fill=(8, 10, 14, 70))
    shadow = shadow.filter(ImageFilter.GaussianBlur(4.5))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")
    frame = (98, 110, 124, 255)
    frame_dark = (66, 76, 88, 255)
    neon = (92, 218, 236, 255)

    # Central UAV body.
    d.ellipse((cx - 20, cy - 20, cx + 20, cy + 20), fill=frame)
    d.ellipse((cx - 14, cy - 14, cx + 14, cy + 14), fill=(130, 142, 158, 255))

    # Futuristic but simple quad-rotor arms.
    arms = [
        ((cx - 14, cy - 14), (cx - 42, cy - 42)),
        ((cx + 14, cy - 14), (cx + 42, cy - 42)),
        ((cx - 14, cy + 14), (cx - 42, cy + 42)),
        ((cx + 14, cy + 14), (cx + 42, cy + 42)),
    ]
    for a, b in arms:
        d.line([a, b], fill=frame_dark, width=8)

    # Rotors kept bold for clear tiny silhouette.
    for rx, ry in [(-48, -48), (48, -48), (-48, 48), (48, 48)]:
        d.ellipse((cx + rx - 16, cy + ry - 16, cx + rx + 16, cy + ry + 16), fill=(58, 68, 82, 255))
        d.ellipse((cx + rx - 7, cy + ry - 7, cx + rx + 7, cy + ry + 7), fill=(118, 132, 150, 255))

    # Sensor camera pod.
    d.rounded_rectangle((cx - 10, cy + 14, cx + 10, cy + 34), radius=5, fill=frame_dark)
    d.ellipse((cx - 7, cy + 20, cx + 7, cy + 34), fill=(30, 36, 46, 255))
    d.ellipse((cx - 3, cy + 24, cx + 3, cy + 30), fill=neon)

    # Simple futuristic accents.
    d.line([(cx - 8, cy - 2), (cx + 8, cy - 2)], fill=neon, width=2)
    d.line([(cx, cy - 10), (cx, cy + 6)], fill=neon, width=2)

    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon([(cx - 64, cy - 64), (cx + 64, cy - 64), (cx + 64, cy + 64), (cx - 64, cy + 64)], outline=(14, 16, 22, 172), width=2)
    contour = contour.filter(ImageFilter.GaussianBlur(0.45))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    draw_drone(SIZE).save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
