# Biome System

- Data-driven biome definitions in `content/biomes.json`.
- Deterministic biome assignment uses seed + elevation + fertility + pseudo temperature/moisture.
- Biome map stored in `World::biomeMap` and serialized in scenarios as `biomeMap`.
- Renderer colors terrain using biome palette hints.
- Resource nodes are spawned by biome categories (forest, desert/ore, coast/fish proxy, etc.).

## Climate-driven biome assignment

Biome assignment now consumes deterministic climate fields:
- `temperatureMap`: latitude + elevation adjusted heat.
- `moistureMap`: fertility + coast proximity + rain-shadow approximation.

Rain-shadow is approximated by sampling upwind higher terrain and reducing local moisture. This produces regional dry zones near mountain leeward sides and improves desert/steppe transitions.

Hydrology feeds biome richness through rivers/lakes, and mountain elevation gates `mountains` / `snow_mountains` selection.
