# Architecture

- **Tech choices locked**: C++20, CMake+Ninja, SDL2, OpenGL, GLM, JSON.
- **Simulation** runs at fixed 20 Hz in `engine/sim`.
- **Rendering** runs variable frame rate in `engine/render`.
- **Platform** loop and input in `engine/platform`.
- **Game** rules/AI/UI in `/game`.

## Frame dataflow
1. Poll input.
2. Accumulate frame time and execute fixed sim ticks.
3. AI updates and submits orders.
4. Render interpolated world state.
5. Draw HUD/debug data.

## Headless + smoke pipeline
- `--headless` bypasses SDL video + OpenGL setup and runs sim-only ticks.
- `--smoke` enables deterministic runtime assertions for:
  - seed-stable map hash,
  - valid unit states (finite values, IDs, in-range positions),
  - territory recompute execution,
  - AI decision execution,
  - no false early win state.
- `--dump-hash` emits deterministic map/state hashes for CI debugging.

## Overlay texture pipeline
- CPU simulation owns grids:
  - fog visibility (`fog`),
  - territory ownership (`territoryOwner`).
- Renderer uploads dirty grids to GPU R8 textures (`territory`, `border`, `fog`) via `glTexSubImage2D`.
- Border texture is generated CPU-side by neighbor owner edge detection.
- Overlay pass uses planar world projection over terrain (XZ in world-space equivalent for this top-down slice) and blends:
  1. territory tint,
  2. borders,
  3. fog darkening (disabled in GOD Mode).

## GOD mode readability path
- GOD Mode disables fog visibility gating and raises zoom cap.
- LOD tiers for units:
  - near: triangle sprite,
  - mid: simplified square sprite,
  - far: icon dot,
  - very far: clustered markers by world bins.
- Selection uses enlarged pick radius at strategic zoom.
