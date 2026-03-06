# Biome System

- Data-driven biome definitions in `content/biomes.json`.
- Deterministic biome assignment uses seed + elevation + fertility + pseudo temperature/moisture.
- Biome map stored in `World::biomeMap` and serialized in scenarios as `biomeMap`.
- Renderer colors terrain using biome palette hints.
- Resource nodes are spawned by biome categories (forest, desert/ore, coast/fish proxy, etc.).
