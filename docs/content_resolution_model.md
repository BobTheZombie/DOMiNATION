# Deterministic Content Resolution Model

## Goal

Ensure renderer/UI presentation resolves deterministically from authoritative state + manifests,
without affecting simulation hash behavior.

## Ordered fallback chain

All production presentation should follow:
- exact content mapping
- civ-specific mapping
- civ-theme mapping
- category mapping
- default fallback

`engine/render/content_resolution.*` implements this logic and tracks debug counters.

## Domains

Counters are grouped by domain:
- Material
- Entity
- City/Region
- Icon

Fallback hits are counted globally as `FALLBACK_COUNT`.

## LOD

LOD tier selection comes only from camera zoom and is tracked via:
- `LOD_NEAR_COUNT`
- `LOD_MID_COUNT`
- `LOD_FAR_COUNT`

## Safety rules

Fallback must:
- never crash
- remain deterministic
- remain coherent
- stay visible via debug counters

## Pipeline validation

Use:
- `tools/validate_content_pipeline.py`
- `tools/blender/validate_asset_conventions.py`

Both scripts validate manifest/schema consistency and naming conventions.
