# Blender Pipeline

This is the only supported art/content pipeline. Legacy generated sprite/icon content (including `content/textures/complete_rts_pack` and `content/textures/icons`) is retired and should not be used.

- Source generated 2D images in `assets_src/concepts/` or civ folders.
- Run `setup_reference_scene.py` to create REF/MODEL/EXPORT collections.
- Model low-poly assets in `MODEL`, duplicate exportable meshes to `EXPORT`.
- Use origin at ground center, +Y up, forward -Z, 1 Blender unit = 1 meter.
- Export via `export_selected_assets.py` to `export/meshes/*.glb` and metadata JSON.
- Optional: `render_preview_icons.py` and `render_billboard_sheet.py`.
- Validate naming + required variants with `validate_asset_conventions.py`.

## Civilization pack integration

`content/asset_manifest.json` and `content/lod_manifest.json` now include civilization-scoped asset IDs for architecture packs, unit visual packs, and UI icon/portrait assets.

Naming convention used by the pack:
- buildings: `<civ>_<family>_a`
- units: `<civ>_<role_or_unique_name>`
- UI: `ui_icon_civ_<civ>`, `ui_emblem_<civ>`, `portrait_diplomacy_<civ>`, `portrait_campaign_<civ>`

Blender exporters should continue emitting deterministic IDs and file paths so generated manifests remain stable across runs.


## Validation

Run `python tools/blender/validate_asset_conventions.py` to verify asset naming/LOD-id format/theme family coverage before export packaging.
## World rendering fallback expectations

When production meshes/textures are missing, export packs should still provide deterministic placeholder compatibility:
- category-correct unit silhouettes
- category-correct structure silhouettes
- civ/theme mapping hooks that resolve to stable fallback IDs

This keeps save/load/replay/hash behavior stable while art assets iterate.

Styles exported from Blender should be registered in `content/asset_manifest.json` + `content/lod_manifest.json`, then mapped into render classes in stylesheet JSON files.
