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

### Concrete content packs

Each civilization pack now contains concrete unique units and buildings wired to authoritative production:
- Rome: legionary / praetorian guard / logistics cohort; castra + forum center.
- China: imperial guard / fire lancer / scholar-engineer; imperial academy + grand granary.
- Europe: musketeer / field howitzer / industrial engineer; integrated steelworks + grand drydock.
- Middle East: camel raider / mamluk guard / caravan master; caravanserai + desert foundry.

UI panels surface owning civilization, resolved definition IDs, unique markers, and icon IDs from deterministic civ content data.

## Expanded civilization roster

New civilization packs: Russia, USA, Japan, EU, UK, Egypt, Tartaria.

Each pack includes data-driven identity coefficients, doctrine weights, and concrete unique content mappings (>=2 units and >=2 buildings per new civilization).

Tartaria is explicitly fictional/alt-history game content and is not presented as a real-world factual civilization claim.

## Ideology and Alliance Blocs
Dynamic ideology alignment and deterministic alliance bloc behavior are now supported. See `docs/ideology_bloc_system.md` and `scenarios/bloc_test.json` for format and validation commands.
