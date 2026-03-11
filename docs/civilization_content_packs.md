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

## Civilization visual coherence
Rome, China, Europe, and Middle East theme mappings now include industrial/strategic building families so house/farm/market/barracks/city center/port/factory chains resolve consistently.

## Complete civilization visual/content pack

All gameplay civilizations now have complete presentation coverage:
- architecture family variants (house, farm, market, barracks, city center, port, factory hub, rail station, defensive tower)
- unit presentation mappings including unique civilization units
- civilization UI pack assets (civ icons, emblems, diplomacy portraits, campaign portraits)
- deterministic fallbacks (`default_<family>`, generic icon, default portrait)

Covered civilizations:
`rome`, `china`, `europe`, `middle_east`, `russia`, `usa`, `japan`, `eu`, `uk`, `egypt`, `tartaria`.

## Deterministic lookup order

For building visual resolution:
1. gameplay family
2. civilization ID theme mapping
3. civilization runtime `themeId` mapping
4. synthesized `civId_family`
5. deterministic fallback `default_family`

The lookup is content-only and does not alter authoritative gameplay state.

## UI accent and fallback presentation
- Civ accent tinting in HUD/panels is derived from civilization runtime bias values and is presentation-only.
- Content icon/portrait IDs shown in polished panels still resolve via deterministic content lookup; missing assets use the same fallback IDs.
- Unique content markers are surfaced in player-facing context cards without changing authoritative unit/building definitions.


## Entity rendering resolution order

Unit/building presentation for map entities resolves deterministically in this order:
1. Gameplay definition id
2. Civilization-specific mapping
3. Theme mapping
4. Category placeholder
5. Fallback icon/silhouette id

Fallbacks are non-fatal and reported through debug counters.

## UI emblem/icon fallback chain
UI presentation resolves in fixed order: exact content icon -> civilization emblem/theme mapping -> category icon -> default fallback icon.
Debug counters expose `ICON_RESOLVE_COUNT`, `MARKER_RESOLVE_COUNT`, `ALERT_RESOLVE_COUNT`, and `PRESENTATION_FALLBACK_COUNT`.


## Settlement presentation theme mapping

Settlement/capital silhouette selection now consumes civilization identity/theme mappings for these packs:
Rome, China, Europe, Middle East, Russia, USA, Japan, EU, UK, Egypt, Tartaria.

Resolution order for city-region presentation:
1. explicit civ-specific settlement shape
2. civ/theme mapped settlement shape
3. generic settlement shape
4. default fallback (debug counter only; no crash)


## Content resolution and fallback

Civ packs resolve through exact mapping, civ mapping, theme mapping, then category/default fallback, to keep renderer/UI deterministic when specific art is missing.
## Civilization world-visual differentiation requirements

World map visual differentiation is deterministic and uses civ + theme mappings for unit/building/settlement presentation.

Minimum supported civilizations in production pass:
- Rome
- China
- Europe
- Middle East
- Russia
- USA
- Japan
- EU
- UK
- Egypt
- Tartaria (fictional / alt-history, visually distinct)

Missing civ-specific assets must resolve via theme/category fallbacks before default fallback IDs.

Civilization-specific visuals can now be authored via stylesheet `civ_overrides`/`theme_overrides` without renderer code edits.
