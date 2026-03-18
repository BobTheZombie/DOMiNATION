# Shader Pipeline

## Scope
This pipeline is intentionally bounded. It adds only two runtime shader programs:
- terrain readability shader
- model readability shader

Everything remains presentation-only. Shader success/failure cannot affect authoritative simulation, save/load, replay, or hash generation.

## Runtime flow
1. `engine/render/shader_program.*` compiles and links GLSL programs and records debug counters.
2. `engine/render/terrain_shader.*` consumes deterministic terrain chunk data plus stylesheet readability inputs to shade landforms under the pitched RTS camera.
3. `engine/render/model_shader.*` consumes resolved model readability uniforms for ambient fill, directional light, rim light, civ tint, emissive emphasis, warning/guardian/industrial emphasis, and damage contrast.
4. Overlay/debug/selection passes stay outside the shader dependency chain where practical.

## Fallback behavior
- Compile/link errors are logged to stderr.
- If the terrain shader is unavailable, the renderer falls back to the previous fixed-function terrain color + overlay readability path.
- If the model shader is unavailable, the renderer falls back to the previous fixed-function runtime model body path.
- Debug counters expose shader program creation, failures, draw counts, and fallback counts.

## Validation
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 800 --dump-hash
./build/rts --headless --scenario scenarios/rail_logistics_test.json --smoke --ticks 1600 --dump-hash
./build/rts --headless --scenario scenarios/industrial_economy_test.json --smoke --ticks 2000 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 1600 --dump-hash
./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 1800 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
python tools/validate_content_pipeline.py
python tools/blender/validate_asset_conventions.py
```
