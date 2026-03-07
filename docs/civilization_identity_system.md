# Civilization Identity System

Civilization identity is fully data-driven from `content/civilizations.json` and deterministic.

## Data model
Each civilization now defines:
- Identity: `civilization_id`, `display_name`, `short_description`, `theme_id`, `era_flavor_tags`, `campaign_tags`.
- Economy: `economyModifiers.resource_gather` multipliers for `Food/Wood/Metal/Wealth/Knowledge/Oil`.
- Military: `unitBonuses` attack/hp/cost/train multipliers by unit family.
- Logistics/Infrastructure: `logisticsModifiers` (`road_bonus`, `rail_bonus`, `supply_bonus`, `trade_route_bonus`, `tunnel_extraction_bonus`).
- Industry: `industryModifiers` (`factory_throughput_bonus`, refined good output modifiers).
- Diplomacy/Doctrine: `diplomacyDoctrine` aggression/alliance/trade/tension response and operation preference weights.
- Unique content: `uniqueUnits`, `uniqueBuildings`, plus doctrine tags.

## Authoritative gameplay hooks
Modifiers are applied in deterministic authoritative systems:
- Resource trickles and base food gain.
- Trade route wealth + knowledge derivative income.
- Supply and rail throughput.
- Industrial throughput and refined output.
- Unit/building production stats and costs.
- AI posture, diplomacy tendency, and operation type preferences.

## Unique content resolution
- Unit and building definitions resolve by family using civilization mapping.
- Resolved definition IDs are authoritative and serialized.
- Selection panel and debug panels expose resolved IDs and uniqueness markers.

## Deterministic validation counters
Additional authoritative counters:
- `UNIQUE_UNITS_PRODUCED`
- `UNIQUE_BUILDINGS_CONSTRUCTED`
- `CIV_DOCTRINE_SWITCHES`
- `CIV_INDUSTRY_OUTPUT`
- `CIV_LOGISTICS_BONUS_USAGE`
- `CIV_OPERATION_COUNT`

These are included in perf output (`--perf` / `--perf-log`) and save/load state.

## Smoke checks
Use `scenarios/civ_test.json` with:
- `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 2600 --dump-hash`
- `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 3000 --dump-hash`
- `./build/rts --headless --scenario scenarios/civ_test.json --threads 1 --hash-only`
- `./build/rts --headless --scenario scenarios/civ_test.json --threads 4 --hash-only`
- `./build/rts --headless --scenario scenarios/civ_test.json --threads 8 --hash-only`
- `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 1500 --save /tmp/civ_save.json --dump-hash`
- `./build/rts --headless --load /tmp/civ_save.json --smoke --ticks 3000 --dump-hash`
