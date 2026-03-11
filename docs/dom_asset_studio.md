# DOM Asset Studio

DOM Asset Studio is a standalone desktop content-authoring companion for DOMiNATION.

## What it is
- A separate executable (`dom_asset_studio`) with SDL2/OpenGL/ImGui shell.
- A dockable editor workspace with Project Browser, Viewport, Inspector, Stylesheet Editor, Log, and Validation panels.
- A preview tool that reuses engine manifest loading plus render stylesheet/content-resolution rules.
- A workflow bridge from authored assets/manifests/stylesheets to engine-ready validated content.

## What it is not
- Not a replacement for Blender modeling/rigging/sculpting.
- Not authoritative gameplay logic editing.
- Not a full level editor.

## Content workflow
1. Author meshes/materials in Blender and export glTF/GLB.
2. Register assets in `content/asset_manifest.json` and `content/lod_manifest.json`.
3. Edit style mappings in `terrain_styles.json`, `unit_styles.json`, `building_styles.json`, and `object_styles.json`.
4. Open Asset Studio, inspect manifest/style data, switch civ/theme/state/LOD variants, and preview resolved render mappings on real asset geometry.
5. Save stylesheet updates and run validation (`tools/validate_content_pipeline.py`) before runtime use.

## Real asset preview capabilities
- Opens `.gltf` and `.glb` files from Project Browser and from the Asset menu path field.
- Auto-resolves preview mesh references through `AssetManager` + `lod_manifest` lookup first, then falls back to direct path opening.
- Inspector reports mesh/material names, bounds, and vertex/index counts.
- Viewport controls include orbit camera, turntable, grid, wireframe, normals, and socket/attachment overlays.
- Variant source toggle supports resolver/default mode, exact-only, and render-class-only inspection without touching gameplay code.

## Shared pipeline behavior
The studio calls the same `engine/render/render_stylesheet.*` and `engine/render/content_resolution.*` logic used by runtime selection rules (exact mapping, render-class default, civ/theme overrides, state and LOD variants, and fallback behavior).

## Validation and safety behavior
- Manifest diagnostics are shown in the Inspector panel.
- Validation panel can execute `python3 tools/validate_content_pipeline.py` and stream output.
- Missing/invalid asset references are reported in log/inspector as non-fatal preview errors.
- Unsupported glTF payloads (for example, missing triangle POSITION accessors) fail safely and keep the studio responsive.
- JSON parse errors and missing references are surfaced in UI instead of crashing.
