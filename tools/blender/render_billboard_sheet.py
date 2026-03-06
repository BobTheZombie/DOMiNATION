#!/usr/bin/env python3
import argparse
from pathlib import Path

try:
    import bpy  # type: ignore
except Exception:
    bpy = None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-id", required=True)
    parser.add_argument("--out", default="assets_processed/atlases/billboards.png")
    args = parser.parse_args()
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    if bpy is None:
        print("bpy unavailable: billboard render skipped")
        return
    obj = bpy.data.objects.get(args.asset_id)
    if obj is None:
        raise RuntimeError(f"Object {args.asset_id} not found")
    bpy.context.scene.render.filepath = args.out
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
