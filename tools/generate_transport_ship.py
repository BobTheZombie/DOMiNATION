#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/units/transport_ship_rts_sprite_256.png")


def draw_transport_ship(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.56

    # Broad water shadow to anchor the ship in an RTS top-down/isometric view.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow, "RGBA")
    sd.ellipse((cx - 102, cy + 10, cx + 102, cy + 72), fill=(10, 12, 16, 102))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.5))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Water tint behind hull improves readability on transparent background previews.
    wake = [(cx - 84, cy - 44), (cx + 94, cy - 44), (cx + 116, cy + 26), (cx - 64, cy + 26)]
    d.polygon(wake, fill=(54, 118, 164, 148))
    for i in range(4):
        y = cy - 38 + i * 11
        d.line([(cx - 66 + i * 2, y), (cx + 92 - i * 2, y + 1)], fill=(132, 196, 228, 120), width=2)

    # Main wooden hull in three faces for simple isometric volume.
    hull_top = [(cx - 78, cy - 18), (cx + 82, cy - 18), (cx + 56, cy + 50), (cx - 102, cy + 50)]
    hull_left = [(cx - 102, cy + 50), (cx - 78, cy - 18), (cx - 78, cy + 30), (cx - 102, cy + 66)]
    hull_right = [(cx + 82, cy - 18), (cx + 56, cy + 50), (cx + 56, cy + 74), (cx + 82, cy + 8)]
    d.polygon(hull_top, fill=(136, 92, 56, 255))
    d.polygon(hull_left, fill=(94, 62, 36, 255))
    d.polygon(hull_right, fill=(106, 70, 42, 255))

    # Bow and stern caps keep the silhouette unmistakably ship-like.
    bow = [(cx + 82, cy - 18), (cx + 96, cy - 8), (cx + 70, cy + 58), (cx + 56, cy + 50)]
    stern = [(cx - 78, cy - 18), (cx - 94, cy - 8), (cx - 118, cy + 58), (cx - 102, cy + 50)]
    d.polygon(bow, fill=(122, 84, 52, 255))
    d.polygon(stern, fill=(84, 54, 32, 255))

    # Deck planks across the cargo area.
    for i in range(10):
        t = i / 9
        x0 = cx - 74 + 14 * t
        x1 = cx + 78 - 14 * t
        y = cy - 13 + t * 55
        d.line([(x0, y), (x1, y)], fill=(92, 58, 34, 220), width=2)

    # Open cargo deck bay with simple crate loadout.
    cargo_bay = [(cx - 28, cy - 2), (cx + 36, cy - 2), (cx + 22, cy + 34), (cx - 42, cy + 34)]
    d.polygon(cargo_bay, fill=(80, 56, 34, 255))

    crates = [
        (cx - 22, cy + 2, 18, 14),
        (cx - 2, cy + 6, 18, 14),
        (cx + 16, cy + 10, 16, 12),
        (cx - 8, cy + 20, 20, 14),
    ]
    for x, y, w, h in crates:
        d.polygon(
            [(x, y), (x + w, y), (x + w - 6, y + h), (x - 6, y + h)],
            fill=(150, 112, 74, 255),
        )
        d.line([(x + 2, y + 2), (x + w - 7, y + h - 2)], fill=(112, 78, 46, 210), width=2)

    # Side rails to frame the transport deck and improve silhouette.
    rail_color = (166, 128, 86, 255)
    d.line([(cx - 76, cy - 16), (cx + 80, cy - 16)], fill=rail_color, width=3)
    d.line([(cx - 99, cy + 49), (cx + 56, cy + 49)], fill=rail_color, width=3)

    # Compact mast + folded sail; keeps transport identity without noisy detail.
    d.line([(cx + 10, cy - 28), (cx + 14, cy + 18)], fill=(76, 50, 28, 255), width=4)
    folded_sail = [(cx + 12, cy - 28), (cx + 44, cy - 16), (cx + 24, cy - 2), (cx + 10, cy - 8)]
    d.polygon(folded_sail, fill=(198, 188, 158, 245))

    # Team-color pennant accent for gameplay readability.
    pennant = [(cx + 13, cy - 28), (cx + 36, cy - 23), (cx + 18, cy - 16)]
    d.polygon(pennant, fill=(44, 124, 224, 255))
    d.line([(cx + 18, cy - 25), (cx + 30, cy - 22)], fill=(164, 214, 255, 230), width=2)

    # Subtle water foam around the hull.
    d.arc((cx - 110, cy + 18, cx - 66, cy + 56), 200, 345, fill=(206, 232, 246, 160), width=2)
    d.arc((cx + 48, cy + 18, cx + 104, cy + 62), 190, 330, fill=(206, 232, 246, 150), width=2)

    # Soft contour pass for a readable RTS silhouette.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [
            (cx - 118, cy + 60),
            (cx - 94, cy - 12),
            (cx + 14, cy - 36),
            (cx + 98, cy - 8),
            (cx + 70, cy + 80),
            (cx - 104, cy + 72),
        ],
        outline=(20, 16, 12, 145),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_transport_ship(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
