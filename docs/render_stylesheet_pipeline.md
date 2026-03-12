# Render Stylesheet Pipeline

Rendering presentation now resolves through JSON stylesheets:

1. exact content mapping
2. civilization override
3. civilization theme override
4. render class default
5. domain default fallback

Files:
- `content/terrain_styles.json`
- `content/unit_styles.json`
- `content/building_styles.json`
- `content/object_styles.json`

Runtime code: `engine/render/render_stylesheet.*`.

The resolver is deterministic and does not mutate simulation state.

## DOM Asset Studio
`dom_asset_studio` reuses this stylesheet resolution pipeline for preview and exports. Style edits should be validated through the Studio Validate panel (runs `tools/validate_content_pipeline.py`) before shipping content.


Studio save/apply flow:
- Use **Apply and Reload** after manifest/LOD/style edits to persist metadata, reload resolver state, and refresh preview chains deterministically.
- Auto reimport (Live Reload menu) also watches external stylesheet/manifest/asset file edits and performs safe targeted refreshes of resolver outputs, preview, catalog, and validation state.
- Dirty local stylesheet/manifest edits block auto-overwrite: Studio logs a skip warning instead of discarding unsaved authoring changes.
- Use **Export Engine-Compatible Content** to run save/apply + validation + package summary generation in one action.


Scene context preview mode reuses the same stylesheet resolver chain while allowing multiple placements in one bounded preview patch; use this to validate biome contrast, variant overrides, and near/mid/far readability without entering gameplay-authoritative tooling.


Style-context selector modes in Studio (default/exact/render-class/civ/theme/state focus) are preview-only resolution filters and do not mutate runtime stylesheet semantics.


Attachment metadata remains non-authoritative gameplay data: hooks/anchors are authored in manifests/stylesheets for renderer/pipeline consumption only, and Studio diagnostics enforce safe failures for missing or invalid references during preview/export.
