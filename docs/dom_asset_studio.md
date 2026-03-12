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
2. Open DOM Asset Studio and create/edit entries in `content/asset_manifest.json` and `content/lod_manifest.json` from **Inspector → Asset Manifest / LOD**.
3. Assign render class, category metadata, civ/theme tags, and attachment profile metadata for selected assets/LODs.
4. Use **Apply Asset->Stylesheet Mapping** to seed/update exact style mappings from manifest metadata, then edit style layers in `terrain_styles.json`, `unit_styles.json`, `building_styles.json`, and `object_styles.json`.
5. Inspect resolved preview variants (domain/civ/theme/state/LOD), open imported glTF/GLB assets, and verify resolved mesh/material/lod references.
6. Run in-studio validation + pipeline validation and use Export to save manifests/stylesheets and package engine-compatible content.

## Real asset preview capabilities
- Opens `.gltf` and `.glb` files from Project Browser and from the Asset menu path field.
- Auto-resolves preview mesh references through `AssetManager` + `lod_manifest` lookup first, then falls back to direct path opening.
- Inspector reports mesh/material names, bounds, and vertex/index counts.
- Viewport controls include orbit camera, turntable, grid, wireframe, normals, and socket/attachment overlays.
- Attachment anchor authoring is available directly in viewport UI: select a socket, adjust 3D anchor position/radius, and write editor-only `attachment_anchors` back into the selected style layer.
- Variant source toggle supports resolver/default mode, exact-only, and render-class-only inspection without touching gameplay code.

## Shared pipeline behavior
The studio calls the same `engine/render/render_stylesheet.*` and `engine/render/content_resolution.*` logic used by runtime selection rules (exact mapping, render-class default, civ/theme overrides, state and LOD variants, and fallback behavior).

## Validation and safety behavior
- Manifest diagnostics are shown in the Inspector panel.
- Validation includes internal checks for duplicate IDs, bad LOD→asset links, missing manifest/style references, stylesheet mesh/LOD mismatches, invalid attachment hooks, empty attachment targets, anchor clipping (outside mesh bounds), and anchor overlap warnings.
- Validation panel can execute `python3 tools/validate_content_pipeline.py` and stream output.
- Export workflow runs save/apply/reload + validation + `tools/asset_pipeline/package_assets.py` packaging and reports status in the Validation panel.
- Missing/invalid asset references are reported in log/inspector as non-fatal preview errors.
- Unsupported glTF payloads (for example, missing triangle POSITION accessors) fail safely and keep the studio responsive.
- JSON parse errors and missing references are surfaced in UI instead of crashing.


## Authoring coverage in this pass
- Structured manifest authoring for asset-facing metadata: `asset_id`, type/category, mesh path, material ref, render class, civ/theme tags, icon/thumbnail refs, notes, and status.
- Structured LOD authoring for `lod_id`/`lod_group_id`, near/mid/far/fallback references, and attachment hook metadata.
- Attachment key inspection and constrained hook editing supports runtime hooks such as `banner_socket`, `civ_emblem`, `smoke_stack`, `muzzle_flash`, `selection_badge`, `warning_badge`, and `guardian_aura`.
- LOD manifest entries now expose structured attachment hook inputs so attachment metadata stays pipeline-authored and safer to serialize/export.
- Apply+Reload workflow keeps preview in sync with runtime-equivalent resolver behavior after edits.
- Saves use temp-file writes followed by replace for safer manifest/stylesheet updates.


## Scene context preview workflow
- Switch **Viewport → Mode** between **Isolated Asset** and **Scene Context**.
- In scene mode, pick a terrain/biome context and use **Place Current Resolved Asset** to stage multiple assets together.
- Use the Scene Outliner to select placements and adjust editor-only transform controls (position/rotation/scale), visibility, and labels.
- Placement actions include duplicate/remove/reset-transform plus reload-selected and reload-all for broken-reference recovery.
- Use **Reset Scene Layout**, **Reload Placed Assets**, and **Clear Scene** for safe iteration.
- Save/load editor-only context layouts in `tools/dom_asset_studio/scene_preview_layout.json` (kept separate from gameplay/scenario content).

## Terrain/biome validation workflow
Scene context includes bounded preview presets for: grassland, plains/steppe, forest ground, desert, mediterranean, jungle, tundra, snow/arctic, wetlands, mountains, snow mountains, and coast/littoral.
Use this to test whether style variants remain legible in expected world environments (for example mine entrances in mountain/snow mountain or port assets in littoral context).

## LOD and RTS readability checks
- Zoom can be validated at tactical/near, mid, and strategic/far camera distance.
- Keep manual LOD selection for explicit testing, or enable **Auto LOD From Zoom** to mirror runtime LOD tier picks.
- Civ/theme/state variant inputs remain active in both viewport modes to compare overrides in context, including attachment/socket mapping previews.
- Style-context selector supports default, exact, render-class-only, civ override focus, theme override focus, and state-variant focus while keeping resolver fallback behavior visible.

## Safety and limitations
- Scene context placement state is editor-only and is not gameplay-authoritative map data.
- Broken/missing mesh references stay listed in the outliner with warnings and do not crash the Studio.
- Scene context preview is intentionally bounded: it is not a level editor, gameplay map editor, or Blender replacement.
