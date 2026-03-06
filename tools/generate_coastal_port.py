#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/buildings/coastal_port_rts_sprite_256.png")


def draw_coastal_port(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = size * 0.5
    cy = size * 0.56

    # Broad shadow gives the whole harbor silhouette readable grounding in RTS view.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow, "RGBA")
    sdraw.ellipse((cx - 108, cy - 6, cx + 108, cy + 70), fill=(12, 14, 18, 96))
    shadow = shadow.filter(ImageFilter.GaussianBlur(6.0))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img, "RGBA")

    # Coastal water wedge behind the harbor to communicate shoreline context.
    water_top = [(cx - 88, cy - 74), (cx + 92, cy - 74), (cx + 114, cy - 20), (cx - 64, cy - 20)]
    d.polygon(water_top, fill=(44, 108, 156, 218))
    for i in range(5):
        y = cy - 68 + i * 10
        d.line([(cx - 70 + i * 2, y), (cx + 88 - i * 2, y + 1)], fill=(104, 178, 214, 130), width=2)

    # Main wooden dock platform.
    dock_top = [(cx - 82, cy - 18), (cx + 78, cy - 18), (cx + 56, cy + 46), (cx - 102, cy + 46)]
    d.polygon(dock_top, fill=(126, 86, 54, 255))

    # Dock planks for unmistakable wooden construction.
    for i in range(11):
        t = i / 10
        x0 = cx - 80 + 16 * t
        x1 = cx + 76 - 16 * t
        y = cy - 14 + t * 56
        d.line([(x0, y), (x1, y)], fill=(94, 62, 36, 210), width=2)

    # Pier extension and ship mooring zone.
    pier = [(cx + 30, cy + 8), (cx + 102, cy + 8), (cx + 86, cy + 60), (cx + 14, cy + 60)]
    d.polygon(pier, fill=(114, 76, 47, 255))
    d.line([(cx + 30, cy + 8), (cx + 14, cy + 60)], fill=(80, 50, 28, 220), width=3)
    d.line([(cx + 102, cy + 8), (cx + 86, cy + 60)], fill=(80, 50, 28, 220), width=3)

    # Mooring posts and ropes (clear ship docking functionality).
    bollards = [(cx + 20, cy + 50), (cx + 44, cy + 56), (cx + 68, cy + 58), (cx + 90, cy + 52)]
    for bx, by in bollards:
        d.rectangle((bx - 3, by - 12, bx + 3, by), fill=(68, 42, 26, 255))
        d.ellipse((bx - 5, by - 13, bx + 5, by - 7), fill=(90, 58, 38, 255))
    d.arc((cx + 26, cy + 42, cx + 62, cy + 60), 188, 340, fill=(188, 170, 132, 230), width=2)
    d.arc((cx + 50, cy + 44, cx + 96, cy + 64), 194, 340, fill=(188, 170, 132, 230), width=2)

    # Two dockside cranes for RTS readability.
    crane_bases = [(cx - 48, cy + 6), (cx - 4, cy + 2)]
    for bx, by in crane_bases:
        # Mast
        d.polygon(
            [(bx - 6, by - 40), (bx + 6, by - 34), (bx + 10, by + 6), (bx - 2, by)],
            fill=(104, 76, 46, 255),
        )
        # Boom arm
        d.polygon(
            [(bx - 6, by - 40), (bx + 30, by - 26), (bx + 24, by - 18), (bx - 9, by - 31)],
            fill=(130, 96, 60, 255),
        )
        # Cable and hook
        hook_x = bx + 24
        hook_y = by - 18
        d.line([(hook_x, hook_y), (hook_x + 2, hook_y + 22)], fill=(52, 50, 48, 255), width=2)
        d.arc((hook_x - 2, hook_y + 18, hook_x + 8, hook_y + 30), 20, 280, fill=(64, 60, 54, 255), width=2)

    # Small warehouse hut on dock to complete port identity.
    hut_top = [(cx - 54, cy - 44), (cx - 8, cy - 44), (cx + 8, cy - 18), (cx - 38, cy - 18)]
    hut_front = [(cx - 38, cy - 18), (cx + 8, cy - 18), (cx + 8, cy + 10), (cx - 38, cy + 10)]
    hut_side = [(cx - 8, cy - 44), (cx + 8, cy - 18), (cx + 8, cy + 10), (cx - 8, cy - 14)]
    d.polygon(hut_top, fill=(146, 120, 84, 255))
    d.polygon(hut_front, fill=(120, 92, 62, 255))
    d.polygon(hut_side, fill=(104, 78, 52, 255))

    roof_l = [(cx - 24, cy - 66), (cx - 62, cy - 44), (cx - 26, cy - 22), (cx - 6, cy - 36)]
    roof_r = [(cx - 24, cy - 66), (cx + 14, cy - 44), (cx - 2, cy - 22), (cx - 6, cy - 36)]
    d.polygon(roof_l, fill=(64, 92, 112, 255))
    d.polygon(roof_r, fill=(52, 76, 94, 255))

    # Naval marker flag reinforces this as a port.
    d.line([(cx + 56, cy - 36), (cx + 60, cy + 2)], fill=(62, 44, 26, 255), width=3)
    flag = [(cx + 58, cy - 34), (cx + 80, cy - 28), (cx + 66, cy - 18), (cx + 58, cy - 22)]
    d.polygon(flag, fill=(22, 132, 198, 255))
    d.line([(cx + 62, cy - 30), (cx + 74, cy - 26)], fill=(186, 232, 248, 200), width=2)

    # Soft contour for silhouette clarity.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cd = ImageDraw.Draw(contour, "RGBA")
    cd.polygon(
        [(cx - 92, cy - 76), (cx + 98, cy - 76), (cx + 108, cy + 62), (cx - 108, cy + 50)],
        outline=(20, 18, 16, 145),
        width=3,
    )
    contour = contour.filter(ImageFilter.GaussianBlur(0.5))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_coastal_port(SIZE)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
