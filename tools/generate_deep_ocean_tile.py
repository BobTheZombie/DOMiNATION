#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image

SIZE = 1024
OUT_PATH = Path("content/textures/terrain/water_deep_ocean_rts_tile_1024.png")
SEED = 710221


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

    noise_layers = [
        (4, 0.40),
        (8, 0.27),
        (16, 0.19),
        (32, 0.10),
        (64, 0.04),
    ]
    grids = {period: build_grid(period, rng) for period, _ in noise_layers}

    # Oriented swells for broad, gentle wave motion.
    swell_dir = rng.uniform(0.09, 0.21) * math.tau
    sx = math.cos(swell_dir)
    sy = math.sin(swell_dir)

    secondary_dir = swell_dir + rng.uniform(0.18, 0.30) * math.tau
    qx = math.cos(secondary_dir)
    qy = math.sin(secondary_dir)

    primary_freq = 3.2
    secondary_freq = 2.1

    # Dark deep-ocean palette (kept much darker than shallow water).
    deep_dark = (8, 30, 62)
    deep_mid = (15, 57, 106)
    deep_light = (24, 90, 146)

    img = Image.new("RGB", (size, size))
    px = img.load()

    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size

            base_noise = 0.0
            for period, weight in noise_layers:
                base_noise += weight * periodic_value_noise(u * period, v * period, period, grids[period])
            base_noise = max(0.0, min(1.0, base_noise))

            swell_axis = (u * sx + v * sy) % 1.0
            cross_axis = (u * qx + v * qy) % 1.0

            primary_wave = 0.5 + 0.5 * math.sin(swell_axis * math.tau * primary_freq)
            secondary_wave = 0.5 + 0.5 * math.sin(cross_axis * math.tau * secondary_freq + 1.3)

            # Gentle broad swells with low contrast for RTS readability.
            wave = 0.68 * primary_wave + 0.32 * secondary_wave
            wave = 0.74 * wave + 0.26 * base_noise

            # Foam appears subtly near upper crest values only.
            foam_mask = max(0.0, min(1.0, (wave - 0.74) / 0.26))
            foam = smoothstep(foam_mask) * (0.20 + 0.30 * periodic_value_noise(u * 32.0, v * 32.0, 32, grids[32]))

            deep_tone = max(0.0, min(1.0, wave * 0.88))
            if deep_tone < 0.58:
                t = deep_tone / 0.58
                r = deep_dark[0] + (deep_mid[0] - deep_dark[0]) * t
                g = deep_dark[1] + (deep_mid[1] - deep_dark[1]) * t
                b = deep_dark[2] + (deep_mid[2] - deep_dark[2]) * t
            else:
                t = (deep_tone - 0.58) / 0.42
                r = deep_mid[0] + (deep_light[0] - deep_mid[0]) * t
                g = deep_mid[1] + (deep_light[1] - deep_mid[1]) * t
                b = deep_mid[2] + (deep_light[2] - deep_mid[2]) * t

            # Foam tint is cool and restrained.
            r = int(max(0.0, min(255.0, r + foam * 22.0)))
            g = int(max(0.0, min(255.0, g + foam * 30.0)))
            b = int(max(0.0, min(255.0, b + foam * 36.0)))
            px[x, y] = (r, g, b)

    # Ensure hard edge parity for true tiling on all engines.
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
