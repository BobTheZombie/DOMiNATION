#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[2]
DIST_DIR = ROOT / "dist"
ARCHIVE = DIST_DIR / "rts_assets.tar.gz"
SUMMARY = DIST_DIR / "package_summary.json"
STUDIO_EXPORT_MANIFEST = DIST_DIR / "studio_export_manifest.json"

INCLUDE_DIRS = [
    ROOT / "assets_final/atlases",
    ROOT / "assets_final/buildings",
    ROOT / "assets_final/units",
    ROOT / "assets_final/ui",
    ROOT / "assets_final/fx",
    ROOT / "content",
]

MANIFESTS = [
    ROOT / "content/asset_manifest.json",
    ROOT / "content/atlas_manifest.json",
    ROOT / "content/biome_manifest.json",
    ROOT / "content/civilization_theme_manifest.json",
    ROOT / "content/lod_manifest.json",
]

STYLE_SHEETS = [
    ROOT / "content/terrain_styles.json",
    ROOT / "content/unit_styles.json",
    ROOT / "content/building_styles.json",
    ROOT / "content/object_styles.json",
]


def count_files(paths: list[Path]) -> int:
    count = 0
    for path in paths:
        if path.is_dir():
            count += sum(1 for p in path.rglob("*") if p.is_file())
        elif path.is_file():
            count += 1
    return count


def main() -> None:
    DIST_DIR.mkdir(parents=True, exist_ok=True)

    with tarfile.open(ARCHIVE, "w:gz") as tar:
        for path in INCLUDE_DIRS:
            if not path.exists():
                continue
            for f in sorted(path.rglob("*")):
                if not f.is_file():
                    continue
                rel = f.relative_to(ROOT)
                if "assets_work" in rel.parts:
                    continue
                if any(part.endswith(".blend") for part in rel.parts):
                    continue
                tar.add(f, arcname=str(rel))

    asset_manifest = json.loads((ROOT / "content/asset_manifest.json").read_text(encoding="utf-8"))
    atlas_manifest = json.loads((ROOT / "content/atlas_manifest.json").read_text(encoding="utf-8"))
    biome_manifest = json.loads((ROOT / "content/biome_manifest.json").read_text(encoding="utf-8"))
    civ_manifest = json.loads((ROOT / "content/civilization_theme_manifest.json").read_text(encoding="utf-8"))

    summary = {
        "asset_count": len(asset_manifest.get("assets", [])),
        "atlas_count": len(atlas_manifest.get("atlases", [])),
        "biomes": len(biome_manifest.get("biomes", [])),
        "civilizations": len(civ_manifest.get("themes", [])),
        "packaged_files": count_files(INCLUDE_DIRS),
        "required_manifest_count": sum(1 for m in MANIFESTS if m.exists()),
        "stylesheet_count": sum(1 for s in STYLE_SHEETS if s.exists()),
    }
    SUMMARY.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    export_manifest = {
        "schema_version": 1,
        "pipeline": "dom_asset_studio_export",
        "content_root": "content",
        "required_manifests": [str(m.relative_to(ROOT)) for m in MANIFESTS if m.exists()],
        "render_stylesheets": [str(s.relative_to(ROOT)) for s in STYLE_SHEETS if s.exists()],
        "archive": str(ARCHIVE.relative_to(ROOT)),
        "summary": str(SUMMARY.relative_to(ROOT)),
    }
    STUDIO_EXPORT_MANIFEST.write_text(json.dumps(export_manifest, indent=2), encoding="utf-8")
    print(f"packaged assets -> {ARCHIVE}")


if __name__ == "__main__":
    main()
