# Procedural world generation

## Pass order
1. **Macro landmass**: builds global land/water shape from low-frequency layered noise and preset-specific masks.
2. **Tectonics**: directional seam uplift creates long ridge systems and basins.
3. **Classification**: terrain class (land/shallow/deep), coastline classification, landmass IDs.
4. **Climate + biome**: deterministic `temperatureMap` / `moistureMap` fields drive biome assignment.
5. **Hydrology**: mountain/highland sources route downhill into rivers and terminal lakes.
6. **Resource geography**: deterministic resource weighting and mountain/deep deposit generation.
7. **Candidate generation**: civ starts/capitals/trade hubs and mythic guardian site candidates.

## Presets
- `pangaea`
- `continents`
- `archipelago`
- `inland_sea`
- `mountain_world`

## Determinism
Worldgen is deterministic for the same:
- seed
- map size
- world preset

Gameplay-relevant maps (land/water, elevation, biome, rivers/lakes, resources, start candidates, mythic candidates) are included in deterministic setup hashing.

## Smoke commands
```bash
./build/rts --headless --smoke --ticks 200 --seed 1234 --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset archipelago --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset mountain_world --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 1 --hash-only
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 4 --hash-only
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 8 --hash-only
```
