# Content Format (JSON)

`game/content/default_content.json` schema:
- `resources: string[]`
- `ages: string[]`
- `cultures: {id:string, bonus:string}[]`
- `buildingDefs: BuildingDef[]`
- `unitDefs: UnitDef[]`
- `ageUp: AgeUpDef`

## BuildingDef
- `id: string` (`CityCenter`, `House`, `Farm`, `LumberCamp`, `Mine`, `Market`, `Library`, `Barracks`, `Wonder`)
- `size: [float, float]` footprint used by placement/collision
- `buildTime: float` seconds to finish construction
- `cost: ResourceCost` optional
- `trickle: ResourceCost` optional passive/operated trickle output
- `popCapBonus: int` optional housing contribution

## UnitDef
- `id: string` (`Worker`, `Infantry`)
- `trainTime: float` seconds in production queue
- `popCost: int`
- `cost: ResourceCost`

## AgeUpDef
- `researchTime: float`
- `cost: ResourceCost`

## ResourceCost
Object with optional keys:
- `food`, `wood`, `metal`, `wealth`, `knowledge`, `oil` (all numeric)

## Notes
- Unlocks by age are currently minimal (age-up implemented, hard gate expansions can be added through future `requiredAge` fields).
- Unknown fields are ignored by current loader.


## Combat role fields (new)
- `role`: one of `INFANTRY`, `RANGED`, `CAVALRY`, `SIEGE`, `WORKER`.
- `attackType`: `melee` or `ranged`.
- `preferredTargetRole`: optional role hint.
- `vsRoleMultiplier`: optional object keyed by role (`INFANTRY`, `RANGED`, `CAVALRY`, `SIEGE`, `WORKER`, `BUILDING`) using float multipliers.

Counter defaults used by current content: infantry>ranged, ranged>cavalry, cavalry>siege/support, siege>building.


## Match rules fields
Optional `matchRules` object:
- `wonderHoldTicks: int`
- `timeLimitTicksDefault: int`
- `scoreTieBreak: "lowestPlayerId"`
- `scoreWeights: {resource:int, unitsAlive:int, buildingsAlive:int, ageReached:int, capitalsControlled:int}`

## Replay format
Replay JSON fields:
- `schemaVersion`, `seed`, `mapWidth`, `mapHeight`, `timeLimitTicks`, `flags`, `contentHash`, `expectedFinalHash`
- `commands[]`: `{tick,type,team,ids?,target?,enemy?,buildingId?,unitType?,buildingType?,queueIndex?}`


## Save format
Authoritative save JSON fields:
- `schemaVersion` (currently `1`, required)
- `seed`, `tick`, `mapWidth`, `mapHeight`
- `heightmap`, `fertility`, `territoryOwner`, `fog`
- `players[]`, `cities[]`, `units[]`, `buildings[]` (including production queues)
- `match`, `config`, `wonder`
- `stateHash` authoritative hash at save tick (used by loader validation)

Excluded from saves:
- flow-field/nav caches
- minimap/renderer/UI transient data
- debug-only counters

See also: `docs/scenario_format.md` and `docs/trigger_format.md` for authored scenario content.
