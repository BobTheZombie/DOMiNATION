# Tooling

## Content pipeline validation

- `python tools/validate_content_pipeline.py`
- `python tools/blender/validate_asset_conventions.py`

## Blender pipeline

- `tools/blender/setup_reference_scene.py`
- `tools/blender/export_selected_assets.py`
- `tools/blender/render_preview_icons.py`
- `tools/blender/render_billboard_sheet.py`


## Runtime asset pipeline

- Build atlases + atlas manifest: `python tools/asset_pipeline/build_atlases.py`
- Package distributable assets: `python tools/asset_pipeline/package_assets.py`
- Validate manifest integrity: `python tools/validate_content_pipeline.py`
