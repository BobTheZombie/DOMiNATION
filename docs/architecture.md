# Architecture

- **Tech choices locked**: C++20, CMake+Ninja, SDL2, OpenGL, GLM, JSON.
- **Simulation** runs at fixed 20 Hz in `engine/sim`.
- **Rendering** runs variable frame rate in `engine/render`.
- **Platform** loop and input in `engine/platform`.
- **Game** rules/AI/UI in `/game`.
- **Data model** uses stable IDs and mostly SoA-friendly vectors for hot data.

Dataflow per frame:
1. Poll input.
2. Accumulate frame time and execute fixed sim ticks.
3. AI updates and submits orders.
4. Render interpolated world state.
5. Draw HUD/debug data.
