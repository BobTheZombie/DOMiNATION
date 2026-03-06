# Scenario format

`schemaVersion: 1` JSON files define authored starts.

Top-level fields:
- `mapWidth`, `mapHeight`, `seed`
- optional `heightmap`/`fertility` grids (authoritative if present)
- `players`, `cities`, `units`, `buildings`, `resourceNodes`
- optional `territoryOwner`
- `rules` (`timeLimitTicks`, `wonderHoldTicks`, `allowConquest`, `allowScore`, `allowWonder`)
- `areas`, `objectives`, `triggers`

When terrain grids are omitted, terrain is generated from `seed` and authored placements are applied on top.

Authoritative: players, entities, resources, triggers/objectives, rules, territory if provided.
Derived: nav cache, fog/territory recompute counters, editor UI state.


## Extended authored schema
- `map.width`, `map.height`
- `players[]`: `id,isHuman,isCPU,civilization,team,color,startingResources,popCap`
- `placements`: `cities,units,buildings,resourceNodes`
- `terrainOverrides` (optional): `height`, `fertility`
- `rulesOverrides`: `timeLimit`, `wonderRules`, `disabledVictoryTypes`


### Logistics additions

Scenario JSON supports:

- `roads`: array of `{id, owner, a:[x,y], b:[x,y], quality}` segments.

`scenarios/logistics_test.json` provides a minimal logistics validation setup.
