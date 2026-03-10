# Render Pipeline Integration Pass

This pass keeps simulation authority unchanged and makes content resolution deterministic and manifest-driven.

## Deterministic resolution model

Resolution order is unified across terrain/entity/UI lookups:
1. exact mapping
2. civilization-specific mapping
3. civilization-theme mapping
4. category mapping
5. default fallback

Renderer counters exposed in debug panels:
- `MATERIAL_RESOLVE_COUNT`
- `ENTITY_RESOLVE_COUNT`
- `CITY_REGION_RESOLVE_COUNT`
- `ICON_RESOLVE_COUNT`
- `FALLBACK_COUNT`
- `LOD_NEAR_COUNT`, `LOD_MID_COUNT`, `LOD_FAR_COUNT`

## Terrain/material pass

`resolve_terrain_visual` maps authoritative terrain and biome inputs to deterministic material classes:
- grassland, steppe, forest ground, desert, mediterranean, jungle, tundra, snow/arctic,
- wetlands/marsh, mountains, snow-capped mountains,
- coast/littoral, shallow water, deep water, river/lake.

Cliff/slope readability and water/material overlays remain presentation-only.

## Entity/city/structure pass

World entities resolve through content presentation metadata and stable fallbacks.
City/region markers (industrial/port/rail/mine/capital) use deterministic category IDs and
remain coherent from near zoom to strategic zoom.

## LOD policy

LOD tier selection is deterministic from camera zoom:
- Near: tactical detail
- Mid: grouped readability
- Far: strategic clusters and markers

## Validation commands

```bash
python tools/validate_content_pipeline.py
python tools/blender/validate_asset_conventions.py
```

## Smoke commands

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
```
