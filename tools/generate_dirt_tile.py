#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image

SIZE = 1024
OUT_PATH = Path("content/textures/terrain/dirt_dry_rts_tile_1024.png")
SEED = 90210


def fract(x: float) -> float:
    return x - math.floor(x)


def smoothstep(t: float) -> float:
    return t * t * (3.0 - 2.0 * t)


def periodic_value_noise(x: float, y: float, period: int, grid: list[list[float]]) -> float:
    x0 = int(math.floor(x)) % period
    y0 = int(math.floor(y)) % period
    x1 = (x0 + 1) % period
    y1 = (y0 + 1) % period

    tx = smoothstep(fract(x))
    ty = smoothstep(fract(y))

    v00 = grid[y0][x0]
    v10 = grid[y0][x1]
    v01 = grid[y1][x0]
    v11 = grid[y1][x1]

    a = v00 * (1.0 - tx) + v10 * tx
    b = v01 * (1.0 - tx) + v11 * tx
    return a * (1.0 - ty) + b * ty


def build_grid(period: int, rng: random.Random) -> list[list[float]]:
    return [[rng.random() for _ in range(period)] for _ in range(period)]


def make_tile(size: int, seed: int) -> Image.Image:
    rng = random.Random(seed)

    # Multi-scale periodic noise layers.
    layers = [
        (8, 0.55),
        (16, 0.25),
        (32, 0.13),
        (64, 0.07),
    ]
    grids = {period: build_grid(period, rng) for period, _ in layers}

    # Track parameters (wrapped sine bands so edges align).
    track_dir = rng.uniform(0.15, 0.35) * math.tau
    tx = math.cos(track_dir)
    ty = math.sin(track_dir)
    track_freq = 3.6
    secondary_freq = 1.1

    # Dry soil palette tuned to contrast with grass.
    dark = (87, 61, 38)
    light = (164, 128, 79)

    img = Image.new("RGB", (size, size))
    px = img.load()

    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size

            n = 0.0
            for period, weight in layers:
                n += weight * periodic_value_noise(u * period, v * period, period, grids[period])

            # Normalize noise to [0,1].
            n = max(0.0, min(1.0, n))

            # Broad, subtle vehicle/foot tracks.
            axis = (u * tx + v * ty) % 1.0
            wobble = 0.06 * math.sin((u + v) * math.tau * secondary_freq)
            base_track = math.sin((axis + wobble) * math.tau * track_freq)
            track = smoothstep(0.5 + 0.5 * base_track)
            track = (track - 0.5) * 0.18

            # Slight compaction: lower micro contrast around tracks.
            compact = 1.0 - 0.13 * smoothstep(0.5 + 0.5 * base_track)
            tone = max(0.0, min(1.0, n * compact + track + 0.08))

            r = int(dark[0] + (light[0] - dark[0]) * tone)
            g = int(dark[1] + (light[1] - dark[1]) * tone)
            b = int(dark[2] + (light[2] - dark[2]) * tone)
            px[x, y] = (r, g, b)

    # Force hard seam parity for engines that sample edge texels directly.
    for y in range(size):
        px[size - 1, y] = px[0, y]
    for x in range(size):
        px[x, size - 1] = px[x, 0]

    return img


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    tile = make_tile(SIZE, SEED)
    tile.save(OUT_PATH, "PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
