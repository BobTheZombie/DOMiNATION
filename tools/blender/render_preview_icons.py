#!/usr/bin/env python3
import argparse
from pathlib import Path

try:
    import bpy  # type: ignore
except Exception:
    bpy = None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", default="assets_processed/icons")
    args = parser.parse_args()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if bpy is None:
        print("bpy unavailable: no icons rendered")
        return
    coll = bpy.data.collections.get("EXPORT")
    if coll is None:
        raise RuntimeError("Missing EXPORT collection")
    for obj in coll.objects:
        if obj.type != 'MESH':
            continue
        bpy.context.scene.camera = bpy.data.objects.get("Camera")
        bpy.context.scene.render.filepath = str(out_dir / f"{obj.name}.png")
        bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
