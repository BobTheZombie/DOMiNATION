#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 256
SEED = 240513
OUT_PATH = Path("content/textures/resources/forest_resource_cluster_rts_sprite_256.png")


CANOPY_COLORS: list[tuple[int, int, int, int]] = [
    (54, 126, 56, 238),
    (67, 144, 61, 240),
    (73, 156, 70, 236),
    (42, 110, 49, 242),
    (82, 168, 78, 232),
]


def clamp_u8(value: float) -> int:
    return int(max(0, min(255, round(value))))


def tint(color: tuple[int, int, int, int], amount: float) -> tuple[int, int, int, int]:
    r, g, b, a = color
    return (
        clamp_u8(r + amount),
        clamp_u8(g + amount),
        clamp_u8(b + amount),
        a,
    )


def draw_tree(
    draw: ImageDraw.ImageDraw,
    rng: random.Random,
    center: tuple[float, float],
    radius: float,
    canopy_color: tuple[int, int, int, int],
) -> None:
    cx, cy = center

    # Trunk peeks out at the lower edge to keep top-down readability.
    trunk_w = radius * 0.40
    trunk_h = radius * 0.46
    draw.ellipse(
        [
            cx - trunk_w,
            cy + radius * 0.30,
            cx + trunk_w,
            cy + radius * 0.30 + trunk_h,
        ],
        fill=(86, 66, 42, 210),
    )

    # Main canopy mass.
    draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=canopy_color)

    # Lobe blobs for stylized dense foliage without becoming an indistinct blob.
    lobe_count = rng.randint(3, 5)
    for i in range(lobe_count):
        ang = (i / lobe_count) * math.tau + rng.uniform(-0.35, 0.35)
        lobe_r = radius * rng.uniform(0.34, 0.52)
        orbit = radius * rng.uniform(0.45, 0.66)
        lx = cx + math.cos(ang) * orbit
        ly = cy + math.sin(ang) * orbit * 0.92
        draw.ellipse(
            [lx - lobe_r, ly - lobe_r, lx + lobe_r, ly + lobe_r],
            fill=tint(canopy_color, rng.uniform(-12, 10)),
        )

    # Top highlight and lower occlusion to pop from RTS camera distance.
    draw.ellipse(
        [
            cx - radius * 0.55,
            cy - radius * 0.78,
            cx + radius * 0.35,
            cy - radius * 0.12,
        ],
        fill=(180, 216, 148, 42),
    )
    draw.pieslice(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        start=18,
        end=162,
        fill=(24, 50, 26, 56),
    )


def make_forest_cluster(size: int, seed: int) -> Image.Image:
    rng = random.Random(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    center = (size * 0.5, size * 0.53)

    # Ground shadow keeps the sprite anchored while preserving transparent edges.
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow, "RGBA")
    shadow_draw.ellipse(
        [center[0] - 78, center[1] - 54, center[0] + 78, center[1] + 54],
        fill=(20, 34, 20, 88),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=5.5))
    img.alpha_composite(shadow)

    draw = ImageDraw.Draw(img, "RGBA")

    # Hand-tuned offsets to stay dense but still readable from top-down zoom.
    tree_layout = [
        (-42, -18, 24),
        (-13, -28, 21),
        (18, -25, 22),
        (43, -10, 23),
        (-49, 12, 22),
        (-20, 12, 24),
        (12, 10, 23),
        (40, 16, 21),
        (-30, 37, 20),
        (3, 36, 22),
        (31, 37, 20),
    ]

    # Draw back-to-front by Y to maintain top-down layering clarity.
    tree_layout.sort(key=lambda item: item[1] - item[2] * 0.15)

    for ox, oy, radius in tree_layout:
        jitter_x = rng.uniform(-2.3, 2.3)
        jitter_y = rng.uniform(-2.0, 2.0)
        tree_center = (center[0] + ox + jitter_x, center[1] + oy + jitter_y)
        canopy = rng.choice(CANOPY_COLORS)
        draw_tree(draw, rng, tree_center, radius + rng.uniform(-1.1, 1.4), canopy)

    # Sparse edge shrubs break hard silhouette while keeping cluster readable.
    for _ in range(8):
        ang = rng.uniform(0.0, math.tau)
        dist = rng.uniform(70, 92)
        sx = center[0] + math.cos(ang) * dist
        sy = center[1] + math.sin(ang) * dist * 0.82
        rad = rng.uniform(4.0, 6.8)
        color = rng.choice(CANOPY_COLORS)
        draw.ellipse([sx - rad, sy - rad, sx + rad, sy + rad], fill=tint(color, -4))

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sprite = make_forest_cluster(SIZE, SEED)
    sprite.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
