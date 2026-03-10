# Mountain Warfare System

## Deterministic rules
- Mountain/highland/pass cells now apply bounded movement modifiers by unit class.
- Land combat damage uses bounded mountain/pass modifiers (infantry + ranged perform better in mountains; cavalry/armor and siege are penalized in rough terrain).
- Mountain/passes grant defensive structure reduction to incoming damage for existing defensive anchors (`Barracks`, `RadarTower`, `AABattery`, and `Mine` in mountain zones).
- Tunnel graph authority can now be used for military reroute when units are stuck in a mountain region and own/neutral shafts are connected.

## Chokepoint and pass control
- Pass cells are inferred deterministically from narrow land fronts adjacent to mountain biomes.
- Pass control and contest counters are tracked in authoritative world state.

## AI behavior
- AI can prefer pass objectives for defensive civs/archetypes and place early radar-based strongpoints near pass targets.
- Civ flavor in mountain warfare: Rome favors hold/pass defense, Russia favors artillery-heavy mountain defense, Egypt biases corridor control, Tartaria biases tunnel/mountain-route synergy.

## New counters
- `MOUNTAIN_COMBAT_EVENTS`
- `PASS_CONTROL_EVENTS`
- `TUNNEL_MILITARY_MOVES`
- `MOUNTAIN_FORT_BONUS_EVENTS`
- `CHOKEPOINT_CONTESTS`
- `MOUNTAIN_ROUTE_SELECTIONS`

These are exposed in PERF stream and `--perf-log` CSV columns.
