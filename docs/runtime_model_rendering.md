# Runtime model rendering (bounded pass)

This pass adds a deterministic runtime 3D model integration layer for units, buildings, and strategic world objects.

## Resolution flow
1. `resolve_render_style(...)` picks style via exact ID -> civ override -> theme override -> class/default fallback.
2. The selected style provides `mesh` + `lod_group` plus bounded readability metadata (`readability`) for ambient/directional balance, civ tint strength, emissive accents, terrain blend, damage contrast, and far-zoom boosts.
3. `ModelCache` resolves LOD asset IDs from `content/lod_manifest.json`, mesh paths from `content/asset_manifest.json`, and optional per-asset attachment hook offsets from `asset_manifest.attachment_hooks`.
4. `GltfRuntimeLoader` validates GLB headers and caches loaded metadata.
5. Invalid/missing assets resolve to `assets_final/fallback/missing_mesh.glb` with a deterministic warning.

## LOD behavior
- near: `_lod0`
- mid: `_lod1`
- far: `_lod2` (falls back to group/base asset if not authored)


## Runtime attachment/effect hooks
- `ResolvedRenderStyle::attachments` now drive a deterministic attachment pass for units/buildings/objects.
- Supported semantics: `banner_socket`, `civ_emblem`, `smoke_stack`, `muzzle_flash`, `selection_badge`, `warning_badge`, `guardian_aura`.
- Runtime state wiring (presentation-only):
  - selected units -> `selection_badge`
  - damaged entities / strategic-warning structures -> `warning_badge`
  - active factories and ore/industrial world objects -> `smoke_stack`
  - combat firing window -> `muzzle_flash`
  - guardian units/sites active or revealed -> `guardian_aura`
  - civ/team tint readability on authored hooks -> `banner_socket` + `civ_emblem`
- Hook resolve order is deterministic: asset-manifest hook (`attachment_hooks`) -> semantic hook ID fallback table -> center-offset fallback (non-fatal).
- Missing/unknown hooks fail safely to a deterministic center-offset fallback with debug counting (non-fatal).

## Lighting/material readability
- Runtime model shading is still presentation-only immediate-mode geometry, but now applies deterministic ambient + directional balancing, terrain-aware tint coupling, rim lighting, and emissive accents.
- Industrial, guardian, warning, damage, and civ-identity emphasis are driven from stylesheet readability data plus authoritative presentation state already available to the renderer.
- Far LOD instances receive bounded readability boosts so silhouettes stay legible without drowning out paths, encounter markers, frontlines, or strategic overlays.
- Missing readability fields always fall back to safe per-domain defaults.

## Debug counters
Debug Visualization panel includes:
- `MODEL_RESOLVE_COUNT`
- `MODEL_FALLBACK_COUNT`
- `ACTIVE_MODEL_INSTANCES`
- `LOD_MODEL_TIER_COUNTS`
- `ATTACHMENT_RESOLVE_COUNT`
- `ATTACHMENT_FALLBACK_COUNT`
- `ACTIVE_ATTACHMENT_INSTANCES`
- `TERRAIN_AWARE_INSTANCES`
- `CIV_TINT_INSTANCES`
- `EMISSIVE_ACCENT_INSTANCES`
- `WARNING_HIGHLIGHT_INSTANCES`
- `INDUSTRIAL_HIGHLIGHT_INSTANCES`
- `GUARDIAN_HIGHLIGHT_INSTANCES`
- `DAMAGED_CONTRAST_INSTANCES`
- `FAR_READABILITY_BOOST_INSTANCES`

## Determinism constraints
- Render cache state is transient only (never serialized).
- Authoritative simulation hash is unchanged by this pass.
- Missing assets are non-fatal and resolve through deterministic fallback.

## Runtime animation integration
`RuntimeModelData` caches detected GLB clip names; `draw_model_instance` resolves a bounded clip state per instance and applies lightweight visual pulse timing derived from deterministic playback time. The animation resolver now attempts `requested state -> authored default_state mapping -> default_clip -> direct clip-name match -> first available clip`, so attachment/effect pulses stay deterministic even when authored state mappings are partial. Missing clips and unsupported animation data never crash rendering and always fall back safely.
