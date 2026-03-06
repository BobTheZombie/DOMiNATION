#!/usr/bin/env python3
"""Exports selected objects from EXPORT collection and writes metadata."""
import argparse
import json
from pathlib import Path

try:
    import bpy  # type: ignore
except Exception:
    bpy = None


def export_collection(export_dir: Path, metadata_path: Path, theme: str):
    export_dir.mkdir(parents=True, exist_ok=True)
    assets = []
    coll = bpy.data.collections.get("EXPORT")
    if coll is None:
        raise RuntimeError("Missing EXPORT collection")
    for obj in coll.objects:
        if obj.type != 'MESH':
            continue
        out = export_dir / f"{obj.name}.glb"
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(filepath=str(out), export_format='GLB', use_selection=True)
        assets.append({
            "asset_id": obj.name,
            "type": "building" if "house" in obj.name or "barracks" in obj.name else "prop",
            "civilization_theme": theme,
            "bounds": [float(v) for v in obj.dimensions],
            "material_set": [slot.material.name for slot in obj.material_slots if slot.material],
            "lods": [f"{obj.name}_lod0", f"{obj.name}_lod1"],
            "icon": f"assets_processed/icons/{obj.name}.png"
        })
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_text(json.dumps({"schema_version": 1, "assets": assets}, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--export-dir", default="export/meshes")
    parser.add_argument("--metadata-out", default="content_manifest/export_manifest.json")
    parser.add_argument("--theme", default="neutral")
    args = parser.parse_args()
    if bpy is None:
        Path(args.metadata_out).write_text(json.dumps({"schema_version": 1, "assets": []}, indent=2) + "\n")
        print("bpy unavailable: metadata stub written")
        return
    export_collection(Path(args.export_dir), Path(args.metadata_out), args.theme)


if __name__ == "__main__":
    main()
