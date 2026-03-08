# Camera Navigation System

## Scope
This layer is intentionally non-authoritative. Camera movement, minimap navigation, and zoom behavior live in platform/render presentation code and must not change simulation hashes.

## Features
- Smooth camera pan via WASD + arrow keys.
- Edge scrolling near screen bounds.
- Mouse wheel zoom using a `targetZoom` interpolated into render zoom.
- Middle-mouse drag panning.
- Zoom bands:
  - Tactical (close)
  - Operational (mid)
  - Strategic (far)
  - GOD-view (very far; when GOD mode is enabled)
- Minimap click-to-center + drag-to-reposition.
- Focus helpers:
  - selection (`F`)
  - capital (`Home`)
  - objective (`J`)
  - crisis (`K`)
  - guardian (`L`)
  - theater target (`Y`)
  - strategic alert source cycling (`,` / `.`)

## Determinism contract
- No camera/input values are serialized into authoritative save/hash paths.
- Headless runs remain the source of truth for deterministic validation.
- Thread count parity tests must continue to produce the same authoritative hash values as before this presentation update.

## Integration points
- `engine/platform/app.cpp`: input routing, focus helpers, camera smoothing and clamping.
- `engine/render/renderer.cpp`: world/screen transforms, minimap mapping, minimap viewport rendering.
- `README.md`, `docs/architecture.md`, `docs/ui_system.md`: user-facing controls and architecture behavior.
