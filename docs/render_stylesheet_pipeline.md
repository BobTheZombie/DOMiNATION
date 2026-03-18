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

## Runtime model selection
Resolved styles now directly feed runtime model selection (`mesh`, `lod_group`) for unit/building/object passes with fixed LOD tier mapping and deterministic fallback.


## Runtime attachment consumption
- Attachment maps authored in stylesheets are consumed during the runtime model pass after mesh/LOD resolution.
- Runtime hook positions can optionally be authored per asset in `content/asset_manifest.json` under `attachment_hooks` (semantic or hook ID key), with deterministic fallback to built-in defaults.
- Attachment semantics are presentation-only hooks and do not mutate simulation state.
- Attachment iteration is key-sorted at runtime to keep deterministic ordering across runs.
- Unknown hook identifiers use deterministic fallback offsets and increment attachment fallback diagnostics.

## Animation fields
Entity styles can include an `animation` object with `default_state`, `default_clip`, `state_clips`, and `playback_hints` (`loop`/`oneshot`). Resolution order remains exact -> civ override -> theme override -> render class -> default, then state/lod overlays.


## Runtime semantic attachment consumption
- Author `attachments` in unit/building/object styles to opt specific classes or exact IDs into runtime semantic hooks.
- Hook placement resolves through `content/asset_manifest.json` `attachment_hooks` when present; otherwise renderer uses deterministic semantic defaults and counts fallback usage for debug overlays.
- Attachment pass order is stable because runtime iteration is key-sorted and uses resolved asset IDs + stylesheet-selected hook IDs only.
