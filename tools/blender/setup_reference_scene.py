#!/usr/bin/env python3
"""Blender scene bootstrap for reference-based RTS modeling workflow."""
import argparse
import json
from pathlib import Path

try:
    import bpy  # type: ignore
except Exception:  # allows dry-run in CI
    bpy = None


def ensure_collection(name: str):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    return coll


def configure_scene():
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512


def add_reference_planes(image_paths):
    ref_coll = ensure_collection("REF")
    for idx, image_path in enumerate(image_paths):
        img = bpy.data.images.load(str(image_path))
        mesh = bpy.data.meshes.new(f"ref_plane_{idx}_mesh")
        obj = bpy.data.objects.new(f"ref_plane_{idx}", mesh)
        ref_coll.objects.link(obj)
        obj.location = (idx * 3.0, 0.0, 1.0)
        obj.scale = (1.0, 0.01, 1.0)
        mat = bpy.data.materials.new(name=f"ref_mat_{idx}")
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        tex = nodes.new(type='ShaderNodeTexImage')
        tex.image = img
        bsdf = nodes.get("Principled BSDF")
        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        obj.data.materials.append(mat)


def write_scene_manifest(out_path: Path, asset_id: str, theme: str):
    data = {
        "asset_id": asset_id,
        "collection_export": "EXPORT",
        "theme": theme,
        "unit_scale": 1.0,
        "forward_axis": "-Z",
        "up_axis": "Y"
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-id", required=True)
    parser.add_argument("--theme", default="neutral")
    parser.add_argument("--manifest-out", default="content_manifest/scene_manifest.json")
    parser.add_argument("images", nargs="*")
    args = parser.parse_args()

    write_scene_manifest(Path(args.manifest_out), args.asset_id, args.theme)
    if bpy is None:
        print("bpy unavailable: wrote manifest only")
        return

    configure_scene()
    ensure_collection("MODEL")
    ensure_collection("EXPORT")
    if args.images:
        add_reference_planes([Path(p) for p in args.images])


if __name__ == "__main__":
    main()
