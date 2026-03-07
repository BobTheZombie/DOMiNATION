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


## Naval/coast additions
- Optional `waterMask`: byte grid matching `map.width * map.height` with terrain classes (`0=land`, `1=shallow`, `2=deep`).
- New building: `Port`.
- New units: `TransportShip`, `LightWarship`, `HeavyWarship`, `BombardShip`.

## Diplomacy/geopolitics additions

- `worldTension`: initial global tension value.
- `diplomacyRelations`: array entries `{a, b, relation}` where `relation` is `Allied|Neutral|War|Ceasefire`.
- `treaties`: array entries `{a, b, alliance, tradeAgreement, openBorders, nonAggression, lastChangedTick}`.
- `strategicPosture`: optional initial posture labels per player (`EXPANSIONIST|DEFENSIVE|TRADE_FOCUSED|ESCALATING|TOTAL_WAR`).
- `espionageOps`: optional pre-seeded operation list with `{id, actor, target, type, startTick, durationTicks, state, effectStrength}` where `type` is `RECON_CITY|REVEAL_ROUTE|SABOTAGE_ECONOMY|SABOTAGE_SUPPLY|COUNTERINTEL`.

Reference sample: `scenarios/diplomacy_test.json`.

## Editor round-trip notes
- Scenario editor save/load uses the deterministic scenario ingest/export path (`load_scenario_file` / `save_scenario_file`).
- Authored fields currently include map dimensions, players, units, cities, buildings, resource nodes, roads, biome map, trigger areas/objectives/triggers, diplomacy/treaties/world tension where present.
- Unsupported authoring fields are surfaced in-editor; they are not silently mutated by panel-only UI state.
- Round-trip expectation: save -> reload -> resave keeps stable human-readable JSON with minimal unrelated churn.

## Strategic warfare extensions
- Optional `airUnits` array stores authoritative air missions (`id`, `team`, `class`, `state`, `pos`, `missionTarget`, `hp`, `speed`, `cooldownTicks`, `missionPerformed`).
- Optional `strategicStrikes` array stores strike prep/travel/interception state.
- New placeable building types: `RadarTower`, `MobileRadar`, `Airbase`, `MissileSilo`, `AABattery`, `AntiMissileDefense`.
- Optional `denialZones` array persists temporary strategic denial effects.

## Campaign mission additions

`mission` block: `title`, `briefing`, `introMessages`, `victoryOutcome`, `defeatOutcome`, `partialOutcome`, `branchKey`, `luaScript` or `luaInline`.

`objectives[]` now supports `objective_id`, `description`, `category`, `visible`, `progressText`, `progressValue`.

`triggers[]` conditions include objective/diplomacy/tension/strike hooks; actions include objective state changes, messages, spawns, diplomacy/tension changes, operation launch, end match, and `RunLuaHook`.


## Civilization fields

`players[].civilization` binds a player to a civilization definition. Scenario smoke fixture `scenarios/civ_test.json` demonstrates civ-vs-civ doctrine divergence and unique replacements.

## Mountain mining extensions
Optional fields:
- `mountainRegions`
- `surfaceDeposits`
- `deepDeposits`
- `undergroundNodes`
- `undergroundEdges`

See `scenarios/mountain_mining_test.json`.

## Mythic guardians

Optional root object:

- `mythicGuardians.sites[]`
  - `instance_id` (u32)
  - `guardian_id` (string)
  - `site_type` (`yeti_lair`, `abyssal_trench`, `dune_nest`, `sacred_grove`, `frozen_cavern`)
  - `pos` ([x, y])
  - optional runtime seed state (`discovered`, `alive`, `owner`, `site_active`, `site_depleted`, `spawned`, `behavior_state`, `cooldown_ticks`, `one_shot_used`, `scenario_placed`)
- `mythicGuardians.counters` (optional): `discovered`, `spawned`, `joined`, `killed`, `hostile_events`, `allied_events`

See `scenarios/mythic_guardians_test.json` and `scenarios/mythic_guardians_multi_test.json` for authored examples.

## World generation fields

Scenario files now support and/or emit:
- `worldPreset` (`pangaea|continents|archipelago|inland_sea|mountain_world`)
- `temperatureMap`, `moistureMap`
- `coastClassMap`, `landmassIdByCell`
- `riverMap`, `lakeMap`
- `resourceWeightMap`
- `startCandidates[]` (`cell`, `score`, `civBiasMask`)
- `mythicCandidates[]` (`siteType`, `cell`, `score`)

`seed + map size + worldPreset` remain the deterministic source of truth. Overrides are optional for authored/scenario-locked maps.


## Rail logistics fields

Optional fields for industrial logistics scenarios:
- `railNodes`: `{id, owner, type: Junction|Station|Depot, tile:[x,y], networkId, active}`
- `railEdges`: `{id, owner, aNode, bNode, quality, bridge, tunnel, disrupted}`
- `railNetworks`: `{id, owner, nodeCount, edgeCount, active}`
- `trains`: `{id, owner, type: Supply|Freight|Armored, state, currentNode, destinationNode, currentEdge, routeCursor, segmentProgress, speed, cargo, capacity, cargoType, lastRouteTick, route:[{edgeId,toNode}]}`

Reference scenario: `scenarios/rail_logistics_test.json`.

## Campaign-linked scenarios

Scenarios used by campaigns remain standard scenario files. Campaign carryover is injected into mission startup (player civ/age/resources/world tension + script-visible flags/variables) and mission outcomes drive campaign progression.

## Industrial economy additions
- `players[].refinedGoods`: object with `steel`, `fuel`, `munitions`, `machine_parts`, `electronics`.
- `buildings[].factory`: optional authoritative factory state (`recipeIndex`, `cycleProgress`, `inputBuffer`, `outputBuffer`, `paused`, `blocked`, `active`, `throughputBonus`).
- `industrialRecipes`: optional per-scenario recipe array with fields `output`, `outputAmount`, `cycleTime`, `inputResources` (array), `inputGoods` (array).

Example scenario: `scenarios/industrial_economy_test.json`.


## Theater operations fields

Optional fields for authored operational state:

- `theaterCommands[]`: `{theaterId, owner, bounds:[minX,minY,maxX,maxY], priority, activeOperations[], assignedArmyGroups[], assignedNavalTaskForces[], assignedAirWings[], supplyStatus, threatLevel}`
- `armyGroups[]`: `{id, owner, theaterId, unitIds[], stance, assignedObjective, active}`
- `navalTaskForces[]`: `{id, owner, theaterId, unitIds[], mission, assignedObjective, active}`
- `airWings[]`: `{id, owner, theaterId, squadronIds[], mission, assignedObjective, active}`
- `operationalObjectives[]`: `{id, owner, theaterId, objectiveType, targetRegion:[minX,minY,maxX,maxY], requiredForce, startTick, durationTicks, outcome, active, armyGroups[], navalTaskForces[], airWings[]}`

Reference scenario: `scenarios/theater_operations_test.json`.
