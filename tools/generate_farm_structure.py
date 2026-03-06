#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/buildings/farm_structure_rts_sprite_256.png")


def draw_farm_structure(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.55

    # Broad shadow to anchor both barn and fields as one structure footprint.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow, "RGBA")
    sdraw.ellipse((cx - 98, cy - 20, cx + 98, cy + 62), fill=(24, 20, 10, 95))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.6))
    img.alpha_composite(shadow)

    draw = ImageDraw.Draw(img, "RGBA")

    # Farmland base patch (isometric) to clearly tie barn + crop rows together.
    field_top = [
        (cx - 84, cy - 22),
        (cx + 84, cy - 22),
        (cx + 58, cy + 58),
        (cx - 58, cy + 58),
    ]
    draw.polygon(field_top, fill=(116, 94, 56, 250))

    # Crop rows: strong alternating bands for RTS readability.
    row_colors = [(140, 116, 68, 235), (98, 78, 46, 228)]
    row_count = 10
    for i in range(row_count):
        t0 = i / row_count
        t1 = (i + 1) / row_count
        y0 = (cy - 18) + (cy + 54 - (cy - 18)) * t0
        y1 = (cy - 18) + (cy + 54 - (cy - 18)) * t1
        inset0 = 8 + t0 * 16
        inset1 = 8 + t1 * 16
        strip = [
            (cx - 78 + inset0, y0),
            (cx + 78 - inset0, y0),
            (cx + 78 - inset1, y1),
            (cx - 78 + inset1, y1),
        ]
        draw.polygon(strip, fill=row_colors[i % 2])

    # Green sprouts to reinforce farm association.
    for i in range(8):
        x0 = cx - 56 + i * 15
        draw.line([(x0, cy + 8), (x0 + 4, cy + 18)], fill=(84, 142, 68, 210), width=2)
        draw.line([(x0 + 2, cy + 21), (x0 + 6, cy + 31)], fill=(90, 150, 70, 210), width=2)

    # Barn body.
    barn_top = [
        (cx - 42, cy - 66),
        (cx + 26, cy - 66),
        (cx + 42, cy - 34),
        (cx - 26, cy - 34),
    ]
    barn_front = [
        (cx - 26, cy - 34),
        (cx + 42, cy - 34),
        (cx + 42, cy + 6),
        (cx - 26, cy + 6),
    ]
    barn_side = [
        (cx + 26, cy - 66),
        (cx + 42, cy - 34),
        (cx + 42, cy + 6),
        (cx + 26, cy - 24),
    ]
    draw.polygon(barn_top, fill=(136, 36, 30, 255))
    draw.polygon(barn_front, fill=(166, 52, 44, 255))
    draw.polygon(barn_side, fill=(112, 30, 24, 255))

    # Wood beams across barn walls.
    beam = (86, 56, 30, 255)
    for x in (-20, -4, 12, 28):
        draw.line([(cx + x, cy - 32), (cx + x, cy + 4)], fill=beam, width=4)
    draw.line([(cx - 22, cy - 16), (cx + 40, cy - 16)], fill=beam, width=4)

    # Roof planes and ridge.
    roof_left = [
        (cx - 6, cy - 92),
        (cx - 48, cy - 66),
        (cx - 8, cy - 38),
        (cx + 8, cy - 58),
    ]
    roof_right = [
        (cx - 6, cy - 92),
        (cx + 36, cy - 66),
        (cx + 24, cy - 38),
        (cx + 8, cy - 58),
    ]
    draw.polygon(roof_left, fill=(84, 110, 128, 255))
    draw.polygon(roof_right, fill=(66, 88, 108, 255))
    draw.line([(cx - 6, cy - 92), (cx + 8, cy - 58)], fill=(176, 192, 202, 220), width=2)

    # Barn door.
    draw.rectangle((cx + 6, cy - 8, cx + 26, cy + 6), fill=(62, 36, 18, 255))
    draw.line([(cx + 16, cy - 8), (cx + 16, cy + 6)], fill=(104, 72, 40, 220), width=2)

    # Hay storage stack near barn.
    hay_shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    hs = ImageDraw.Draw(hay_shadow, "RGBA")
    hs.ellipse((cx - 86, cy - 4, cx - 20, cy + 36), fill=(0, 0, 0, 70))
    hay_shadow = hay_shadow.filter(ImageFilter.GaussianBlur(2.2))
    img.alpha_composite(hay_shadow)

    hay_color = (202, 168, 86, 255)
    draw.ellipse((cx - 82, cy - 10, cx - 48, cy + 18), fill=hay_color)
    draw.ellipse((cx - 56, cy - 8, cx - 20, cy + 20), fill=(186, 154, 78, 255))
    draw.ellipse((cx - 70, cy + 4, cx - 34, cy + 30), fill=(172, 142, 72, 255))
    for y in range(0, 24, 5):
        draw.line([(cx - 80, cy - 6 + y), (cx - 22, cy + 18 + y * 0.1)], fill=(140, 108, 54, 190), width=2)

    # Light contour to preserve silhouette over terrain textures.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cdraw = ImageDraw.Draw(contour, "RGBA")
    cdraw.polygon(
        [
            (cx - 88, cy - 24),
            (cx - 6, cy - 98),
            (cx + 46, cy - 32),
            (cx + 62, cy + 62),
            (cx - 60, cy + 62),
        ],
        outline=(26, 20, 12, 130),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_farm_structure(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
