# Asset Naming Conventions

Use lowercase snake naming with deterministic prefixes:
- `civ_rome_house_a`
- `civ_china_barracks_b`
- `biome_desert_market_a`
- `neutral_fish_node_a`

Required building outputs:
- base model
- optional under-construction variant
- optional damaged variant
- icon/preview render

Resource nodes require world model + icon.
LOD strategy: include at least `_lod1` simplified variant for major assets.
