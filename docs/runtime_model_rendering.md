# Runtime model rendering (bounded pass)

This pass adds a deterministic runtime 3D model integration layer for units, buildings, and strategic world objects.

## Resolution flow
1. `resolve_render_style(...)` picks style via exact ID -> civ override -> theme override -> class/default fallback.
2. The selected style provides `mesh` + `lod_group`.
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
  - selected -> `selection_badge`
  - damaged / strategic warning -> `warning_badge`
  - active industry -> `smoke_stack`
  - combat firing window -> `muzzle_flash`
  - guardian site active/revealed -> `guardian_aura`
- Hook resolve order is deterministic: asset-manifest hook (`attachment_hooks`) -> semantic hook ID fallback table -> center-offset fallback (non-fatal).
- Missing/unknown hooks fail safely to a deterministic center-offset fallback with debug counting (non-fatal).

## Debug counters
Debug Visualization panel includes:
- `MODEL_RESOLVE_COUNT`
- `MODEL_FALLBACK_COUNT`
- `ACTIVE_MODEL_INSTANCES`
- `LOD_MODEL_TIER_COUNTS`
- `ATTACHMENT_RESOLVE_COUNT`
- `ATTACHMENT_FALLBACK_COUNT`
- `ACTIVE_ATTACHMENT_INSTANCES`

## Determinism constraints
- Render cache state is transient only (never serialized).
- Authoritative simulation hash is unchanged by this pass.
- Missing assets are non-fatal and resolve through deterministic fallback.
