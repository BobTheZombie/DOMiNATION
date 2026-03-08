# Civilization Content Packs

This document defines concrete civilization content packs and deterministic resolution behavior.

## Resolution model

1. Gameplay chooses a family (`UnitType`/`BuildingType`).
2. Civilization mapping resolves to a concrete definition ID when present.
3. Fallback uses the family base ID when mapping is missing.
4. Resolved ID is authoritative and serialized (`definitionId`).

## Current packs

- Rome: `rome_legionary`, `rome_praetorian_guard`, `rome_logistics_cohort`; `rome_castra`, `rome_forum_centre`.
- China: `china_imperial_guard`, `china_fire_lancer`, `china_scholar_engineer`; `china_imperial_academy`, `china_grand_granary`.
- Europe: `europe_musketeer`, `europe_field_howitzer`, `europe_industrial_engineer`; `europe_integrated_steelworks`, `europe_grand_drydock`.
- Middle East: `middle_east_camel_raider`, `middle_east_mamluk_guard`, `middle_east_caravan_master`; `middle_east_caravanserai`, `middle_east_desert_foundry`.

## Counters

- `UNIQUE_UNITS_PRODUCED`
- `UNIQUE_BUILDINGS_CONSTRUCTED`
- `CIV_CONTENT_RESOLUTION_FALLBACKS`
- `ROME_CONTENT_USAGE`
- `CHINA_CONTENT_USAGE`
- `EUROPE_CONTENT_USAGE`
- `MIDDLEEAST_CONTENT_USAGE`

These are available in debug and perf output for smoke validation.
