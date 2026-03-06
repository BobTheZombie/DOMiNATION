#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
OUT_PATH = Path("content/textures/buildings/medieval_house_rts_sprite_256.png")


def lerp(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * t))


def lerp_color(c0: tuple[int, int, int, int], c1: tuple[int, int, int, int], t: float) -> tuple[int, int, int, int]:
    return tuple(lerp(c0[i], c1[i], t) for i in range(4))  # type: ignore[return-value]


def draw_house(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    center_x = size * 0.5
    center_y = size * 0.54

    # Soft footprint shadow for readability.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow, "RGBA")
    sdraw.ellipse((center_x - 76, center_y + 14, center_x + 76, center_y + 60), fill=(18, 16, 12, 95))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5.0))
    img.alpha_composite(shadow)

    draw = ImageDraw.Draw(img, "RGBA")

    # Stone plinth (isometric diamond).
    base_top = [
        (center_x, center_y - 40),
        (center_x + 66, center_y - 6),
        (center_x, center_y + 28),
        (center_x - 66, center_y - 6),
    ]
    draw.polygon(base_top, fill=(122, 116, 108, 255))

    # Front and side stone faces for top-down/iso read.
    front_face = [
        (center_x - 66, center_y - 6),
        (center_x, center_y + 28),
        (center_x, center_y + 46),
        (center_x - 66, center_y + 12),
    ]
    side_face = [
        (center_x + 66, center_y - 6),
        (center_x, center_y + 28),
        (center_x, center_y + 46),
        (center_x + 66, center_y + 12),
    ]
    draw.polygon(front_face, fill=(104, 99, 92, 255))
    draw.polygon(side_face, fill=(90, 86, 81, 255))

    # Main wall footprint on top of the plinth.
    wall_top = [
        (center_x, center_y - 30),
        (center_x + 52, center_y - 4),
        (center_x, center_y + 20),
        (center_x - 52, center_y - 4),
    ]
    draw.polygon(wall_top, fill=(156, 146, 128, 255))

    wall_front = [
        (center_x - 52, center_y - 4),
        (center_x, center_y + 20),
        (center_x, center_y + 42),
        (center_x - 52, center_y + 18),
    ]
    wall_side = [
        (center_x + 52, center_y - 4),
        (center_x, center_y + 20),
        (center_x, center_y + 42),
        (center_x + 52, center_y + 18),
    ]
    draw.polygon(wall_front, fill=(128, 116, 98, 255))
    draw.polygon(wall_side, fill=(115, 104, 89, 255))

    # Timber frame accents.
    timber = (86, 57, 36, 255)
    for offset in (-34, -10, 16, 40):
        draw.line(
            [(center_x + offset, center_y - 16), (center_x + offset * 0.9, center_y + 25)],
            fill=timber,
            width=4,
        )
    draw.line([(center_x - 46, center_y + 6), (center_x + 46, center_y + 6)], fill=timber, width=4)

    # Roof ridge and thatch planes.
    ridge_top = (center_x, center_y - 78)
    ridge_left = (center_x - 66, center_y - 36)
    ridge_right = (center_x + 66, center_y - 36)
    eave_front = (center_x, center_y + 2)

    roof_left = [ridge_top, ridge_left, (center_x - 4, center_y + 2), eave_front]
    roof_right = [ridge_top, ridge_right, (center_x + 4, center_y + 2), eave_front]
    draw.polygon(roof_left, fill=(178, 148, 78, 255))
    draw.polygon(roof_right, fill=(156, 128, 64, 255))

    # Thatch stroke texture.
    thatch_light = (210, 176, 96, 190)
    thatch_dark = (124, 96, 48, 165)
    for i in range(24):
        t = i / 23
        x0 = lerp(int(ridge_top[0]), int(ridge_left[0]), t)
        y0 = lerp(int(ridge_top[1] + 2), int(ridge_left[1] + 4), t)
        x1 = lerp(int(eave_front[0] - 2), int(center_x - 6), t)
        y1 = lerp(int(eave_front[1]), int(center_y + 3), t)
        color = thatch_light if i % 2 == 0 else thatch_dark
        draw.line([(x0, y0), (x1, y1)], fill=color, width=2)

    for i in range(24):
        t = i / 23
        x0 = lerp(int(ridge_top[0]), int(ridge_right[0]), t)
        y0 = lerp(int(ridge_top[1] + 2), int(ridge_right[1] + 4), t)
        x1 = lerp(int(eave_front[0] + 2), int(center_x + 6), t)
        y1 = lerp(int(eave_front[1]), int(center_y + 3), t)
        color = thatch_light if i % 2 == 1 else thatch_dark
        draw.line([(x0, y0), (x1, y1)], fill=color, width=2)

    # Chimney (stone).
    chimney = [
        (center_x + 14, center_y - 68),
        (center_x + 30, center_y - 60),
        (center_x + 30, center_y - 34),
        (center_x + 14, center_y - 42),
    ]
    draw.polygon(chimney, fill=(110, 106, 102, 255))
    draw.line([(center_x + 14, center_y - 68), (center_x + 30, center_y - 60)], fill=(154, 150, 146, 255), width=2)

    # Team-color banner on a small mast.
    mast_top = (center_x - 46, center_y - 58)
    mast_bottom = (center_x - 42, center_y - 10)
    draw.line([mast_top, mast_bottom], fill=(62, 44, 26, 255), width=3)

    team_color = (34, 132, 238, 255)
    banner = [
        (mast_top[0] + 2, mast_top[1] + 2),
        (mast_top[0] + 26, mast_top[1] + 10),
        (mast_top[0] + 12, mast_top[1] + 22),
        (mast_top[0] + 2, mast_top[1] + 16),
    ]
    draw.polygon(banner, fill=team_color)
    draw.line([banner[1], (banner[1][0] - 4, banner[1][1] + 8), banner[2]], fill=(22, 92, 184, 255), width=2)

    # Contour to keep silhouette crisp over terrain.
    contour = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cdraw = ImageDraw.Draw(contour, "RGBA")
    cdraw.polygon([
        (center_x, center_y - 80),
        (center_x + 72, center_y - 36),
        (center_x + 6, center_y + 48),
        (center_x - 72, center_y + 12),
    ], outline=(30, 24, 16, 150), width=3)
    contour = contour.filter(ImageFilter.GaussianBlur(0.4))
    img.alpha_composite(contour)

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img = draw_house(SIZE)
    img.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
