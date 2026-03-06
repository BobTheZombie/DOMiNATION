#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
SEED = 421337
OUT_PATH = Path("content/textures/resources/ore_resource_node_rts_sprite_256.png")


def clamp_u8(value: float) -> int:
    return int(max(0, min(255, round(value))))


def lerp_color(a: tuple[int, int, int, int], b: tuple[int, int, int, int], t: float) -> tuple[int, int, int, int]:
    return (
        clamp_u8(a[0] + (b[0] - a[0]) * t),
        clamp_u8(a[1] + (b[1] - a[1]) * t),
        clamp_u8(a[2] + (b[2] - a[2]) * t),
        clamp_u8(a[3] + (b[3] - a[3]) * t),
    )


def irregular_polygon(
    rng: random.Random,
    center: tuple[float, float],
    base_radius: float,
    point_count: int,
    jitter: float,
    angle_offset: float,
    x_squash: float,
) -> list[tuple[float, float]]:
    cx, cy = center
    points: list[tuple[float, float]] = []
    for i in range(point_count):
        ang = angle_offset + (i / point_count) * math.tau
        radius = base_radius * (1.0 + rng.uniform(-jitter, jitter))
        px = cx + math.cos(ang) * radius * x_squash
        py = cy + math.sin(ang) * radius
        points.append((px, py))
    return points


def draw_ore_cluster(size: int, seed: int) -> Image.Image:
    rng = random.Random(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    center = (size * 0.5, size * 0.54)

    mound_base_dark = (70, 60, 56, 255)
    mound_base_light = (120, 108, 100, 255)
    mound_shadow = (34, 28, 26, 180)

    # Soft ground shadow for sprite readability.
    shadow_layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow_layer, "RGBA")
    shadow_draw.ellipse(
        [center[0] - 64, center[1] - 40, center[0] + 64, center[1] + 40],
        fill=(24, 22, 20, 94),
    )
    shadow_layer = shadow_layer.filter(ImageFilter.GaussianBlur(radius=4.0))
    img.alpha_composite(shadow_layer)

    draw = ImageDraw.Draw(img, "RGBA")

    # Rocky mound silhouette.
    mound = irregular_polygon(
        rng,
        center,
        base_radius=48,
        point_count=11,
        jitter=0.23,
        angle_offset=rng.uniform(0.0, math.tau),
        x_squash=1.08,
    )
    draw.polygon(mound, fill=mound_base_dark)

    # Layer internal facets for a chunky top-down look.
    for i in range(5):
        facet_center = (
            center[0] + rng.uniform(-20, 20),
            center[1] + rng.uniform(-16, 16),
        )
        facet = irregular_polygon(
            rng,
            facet_center,
            base_radius=rng.uniform(18, 30),
            point_count=rng.randint(5, 8),
            jitter=0.28,
            angle_offset=rng.uniform(0.0, math.tau),
            x_squash=rng.uniform(0.88, 1.2),
        )
        tint = min(1.0, i / 4.0 + rng.uniform(-0.05, 0.1))
        draw.polygon(facet, fill=lerp_color(mound_base_dark, mound_base_light, tint))

    # Lower rim occlusion to preserve silhouette from a top-down camera.
    draw.pieslice(
        [center[0] - 60, center[1] - 52, center[0] + 60, center[1] + 52],
        start=18,
        end=168,
        fill=mound_shadow,
    )

    ore_dark = (108, 118, 126, 240)
    ore_bright = (200, 212, 220, 248)
    ore_specular = (228, 236, 244, 252)

    # Exposed ore seams clustered near top-center to read as valuable deposits.
    seam_centers = [
        (center[0] - 16, center[1] - 10),
        (center[0] + 9, center[1] - 18),
        (center[0] + 22, center[1] + 2),
        (center[0] - 6, center[1] + 14),
    ]

    for idx, seam_center in enumerate(seam_centers):
        seam = irregular_polygon(
            rng,
            seam_center,
            base_radius=rng.uniform(10, 14),
            point_count=rng.randint(5, 7),
            jitter=0.32,
            angle_offset=rng.uniform(0.0, math.tau),
            x_squash=rng.uniform(0.85, 1.18),
        )
        seam_tone = lerp_color(ore_dark, ore_bright, idx / max(1, len(seam_centers) - 1))
        draw.polygon(seam, fill=seam_tone)

        # Crisp silver/iron highlight edge.
        xs = [p[0] for p in seam]
        ys = [p[1] for p in seam]
        bbox = [min(xs), min(ys), max(xs), max(ys)]
        draw.arc(
            bbox,
            start=rng.randint(210, 260),
            end=rng.randint(330, 360),
            fill=ore_specular,
            width=2,
        )

    # Subtle top highlight for material separation.
    draw.ellipse(
        [center[0] - 30, center[1] - 40, center[0] + 26, center[1] - 10],
        fill=(164, 156, 148, 66),
    )

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = draw_ore_cluster(SIZE, SEED)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
