#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image

SIZE = 1024
OUT_PATH = Path("content/textures/terrain/farmland_tile_1024.png")
SEED = 481516


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

    soil_layers = [
        (8, 0.50),
        (16, 0.28),
        (32, 0.15),
        (64, 0.07),
    ]
    grids = {period: build_grid(period, rng) for period, _ in soil_layers}

    row_dir = rng.uniform(0.16, 0.32) * math.tau
    rx = math.cos(row_dir)
    ry = math.sin(row_dir)

    row_freq = 13.0
    furrow_width = 0.36
    crop_width = 0.17

    dark_soil = (83, 58, 36)
    light_soil = (154, 112, 72)
    crop_dark = (74, 128, 46)
    crop_light = (134, 181, 77)

    img = Image.new("RGB", (size, size))
    px = img.load()

    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size

            noise = 0.0
            for period, weight in soil_layers:
                noise += weight * periodic_value_noise(u * period, v * period, period, grids[period])
            noise = max(0.0, min(1.0, noise))

            # Wrapped stripe coordinate for tileable field rows.
            axis = (u * rx + v * ry) % 1.0
            phase = (axis * row_freq) % 1.0

            # Furrows occupy the center of each row interval.
            furrow_dist = abs(phase - 0.5) / 0.5
            furrow = 1.0 - smoothstep(min(1.0, furrow_dist / furrow_width))

            # Crops emerge in twin bands on both sides of each furrow.
            crop_left = abs(phase - (0.5 - crop_width))
            crop_right = abs(phase - (0.5 + crop_width))
            crop_band = min(crop_left, crop_right)
            crop_profile = 1.0 - smoothstep(min(1.0, crop_band / (crop_width * 0.6)))

            # Break perfect lines so rows remain readable but natural at distance.
            row_wobble = periodic_value_noise(
                (u * rx - v * ry) * 18.0,
                (u * ry + v * rx) * 6.0,
                32,
                grids[32],
            )
            crop_strength = max(0.0, min(1.0, crop_profile * (0.72 + 0.45 * row_wobble)))

            soil_tone = max(0.0, min(1.0, noise - furrow * 0.18 + 0.04))
            soil_r = dark_soil[0] + (light_soil[0] - dark_soil[0]) * soil_tone
            soil_g = dark_soil[1] + (light_soil[1] - dark_soil[1]) * soil_tone
            soil_b = dark_soil[2] + (light_soil[2] - dark_soil[2]) * soil_tone

            crop_tone = max(0.0, min(1.0, 0.35 + 0.65 * noise))
            crop_r = crop_dark[0] + (crop_light[0] - crop_dark[0]) * crop_tone
            crop_g = crop_dark[1] + (crop_light[1] - crop_dark[1]) * crop_tone
            crop_b = crop_dark[2] + (crop_light[2] - crop_dark[2]) * crop_tone

            blend = 0.62 * crop_strength
            r = int(soil_r * (1.0 - blend) + crop_r * blend)
            g = int(soil_g * (1.0 - blend) + crop_g * blend)
            b = int(soil_b * (1.0 - blend) + crop_b * blend)

            px[x, y] = (r, g, b)

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
