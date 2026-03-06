# Blender Pipeline

This is the only supported art/content pipeline. Legacy generated sprite/icon content (including `content/textures/complete_rts_pack` and `content/textures/icons`) is retired and should not be used.

- Source generated 2D images in `assets_src/concepts/` or civ folders.
- Run `setup_reference_scene.py` to create REF/MODEL/EXPORT collections.
- Model low-poly assets in `MODEL`, duplicate exportable meshes to `EXPORT`.
- Use origin at ground center, +Y up, forward -Z, 1 Blender unit = 1 meter.
- Export via `export_selected_assets.py` to `export/meshes/*.glb` and metadata JSON.
- Optional: `render_preview_icons.py` and `render_billboard_sheet.py`.
- Validate naming + required variants with `validate_asset_conventions.py`.
