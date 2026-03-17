# RTS Camera System (Pitched 3D)

The runtime camera now uses a constrained RTS rig rather than a pure top-down orthographic view.

## Rig model

- Focus/target point on the world plane (`Camera.center`)
- Yaw (bounded by continuous wrap)
- Pitch (clamped for strategy readability)
- Distance (zoom) with deterministic bounds
- Perspective FOV (bounded)

The rig remains presentation-only and does not participate in authoritative simulation hashing or save state.

## Perspective world projection

- Renderer world pass uses perspective projection + look-at view.
- Picking and `screen_to_world` now ray-cast from camera through the screen into the ground plane.
- Minimap mapping remains world-space based and unchanged in authority semantics.

## Strategy presets

Three bounded presets are provided:

- Close / Tactical
- Mid / Operational
- Far / Strategic

Runtime controls:

- `F6`: close preset
- `F7`: mid preset
- `F8`: far preset
- `F12`: reset yaw + mid preset
- Mouse wheel: smooth distance zoom
- `Q` / `E`: yaw rotate

## Determinism constraints

- Camera state is runtime presentation-only.
- Authoritative simulation updates, save/load, and hash generation remain camera-independent.
