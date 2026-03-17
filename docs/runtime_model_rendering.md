# Runtime model rendering (bounded pass)

This pass adds a deterministic runtime 3D model integration layer for units, buildings, and strategic world objects.

## Resolution flow
1. `resolve_render_style(...)` picks style via exact ID -> civ override -> theme override -> class/default fallback.
2. The selected style provides `mesh` + `lod_group`.
3. `ModelCache` resolves LOD asset IDs from `content/lod_manifest.json` and mesh paths from `content/asset_manifest.json`.
4. `GltfRuntimeLoader` validates GLB headers and caches loaded metadata.
5. Invalid/missing assets resolve to `assets_final/fallback/missing_mesh.glb` with a deterministic warning.

## LOD behavior
- near: `_lod0`
- mid: `_lod1`
- far: `_lod2` (falls back to group/base asset if not authored)

## Debug counters
Debug Visualization panel includes:
- `MODEL_RESOLVE_COUNT`
- `MODEL_FALLBACK_COUNT`
- `ACTIVE_MODEL_INSTANCES`
- `LOD_MODEL_TIER_COUNTS`

## Determinism constraints
- Render cache state is transient only (never serialized).
- Authoritative simulation hash is unchanged by this pass.
- Missing assets are non-fatal and resolve through deterministic fallback.
