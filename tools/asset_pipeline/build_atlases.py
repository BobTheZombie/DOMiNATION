#!/usr/bin/env python3
"""Deterministic atlas metadata builder for processed/final assets."""

from __future__ import annotations

import json
import math
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
ASSETS_FINAL = ROOT / "assets_final"
ATLAS_DIR = ASSETS_FINAL / "atlases"
CONTENT_DIR = ROOT / "content"

CATEGORY_GLOBS = {
    "buildings_atlas": ["buildings/*.png"],
    "units_atlas": ["units/*.png"],
    "ui_atlas": ["ui/*.png"],
    "fx_atlas": ["fx/*.png"],
}

PADDING = 2
SPRITE_SIZE = 256


def next_pow2(v: int) -> int:
    return 1 if v <= 1 else 1 << (v - 1).bit_length()


def discover_pngs(globs: list[str]) -> list[Path]:
    items: list[Path] = []
    for pattern in globs:
        items.extend(sorted(ASSETS_FINAL.glob(pattern)))
    return sorted(set(items))


def build_category(atlas_id: str, pngs: list[Path]) -> tuple[dict, list[dict]]:
    cell = SPRITE_SIZE + PADDING * 2
    cols = max(1, math.ceil(math.sqrt(max(1, len(pngs)))))
    rows = max(1, math.ceil(max(1, len(pngs)) / cols))
    w = next_pow2(cols * cell)
    h = next_pow2(rows * cell)

    ATLAS_DIR.mkdir(parents=True, exist_ok=True)
    atlas_image = ATLAS_DIR / f"{atlas_id}.png"
    if pngs:
        shutil.copyfile(pngs[0], atlas_image)
    else:
        atlas_image.write_bytes(b"")

    sprites: list[dict] = []
    for idx, path in enumerate(pngs):
        x = (idx % cols) * cell + PADDING
        y = (idx // cols) * cell + PADDING
        sprite_id = path.stem
        tags = [atlas_id.replace("_atlas", "")]
        if "unit" in sprite_id:
            tags.append("unit")
        if "building" in sprite_id or atlas_id.startswith("buildings"):
            tags.append("building")
        sprites.append(
            {
                "sprite_id": sprite_id,
                "atlas": atlas_id,
                "rect": [x, y, SPRITE_SIZE, SPRITE_SIZE],
                "pivot": [0.5, 0.5],
                "tags": sorted(set(tags)),
            }
        )

    return (
        {"atlas_id": atlas_id, "image": str(atlas_image.relative_to(ROOT)), "size": [w, h]},
        sprites,
    )


def main() -> None:
    atlases = []
    sprites = []

    for atlas_id, globs in CATEGORY_GLOBS.items():
        discovered = discover_pngs(globs)
        atlas_entry, sprite_entries = build_category(atlas_id, discovered)
        atlases.append(atlas_entry)
        sprites.extend(sprite_entries)

    # deterministic fallback sprite entries
    sprites.extend(
        [
            {"sprite_id": "missing_texture", "atlas": "ui_atlas", "rect": [0, 0, 64, 64], "pivot": [0.5, 0.5], "tags": ["fallback"]},
            {"sprite_id": "missing_icon", "atlas": "ui_atlas", "rect": [64, 0, 64, 64], "pivot": [0.5, 0.5], "tags": ["fallback"]},
        ]
    )

    manifest = {"schema_version": 1, "atlases": atlases, "sprites": sprites}
    CONTENT_DIR.mkdir(parents=True, exist_ok=True)
    with (CONTENT_DIR / "atlas_manifest.json").open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"atlas build complete: atlases={len(atlases)} sprites={len(sprites)}")


if __name__ == "__main__":
    main()
