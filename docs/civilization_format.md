# Civilization format

Civilizations are defined in `content/civilizations.json` as an array.

## Core identity
- `civilization_id`
- `display_name`
- `short_description`
- `theme_id`
- `era_flavor_tags`
- `campaign_tags`

## High-level biases
- `economyBias`, `militaryBias`, `scienceBias`
- `aggression`, `defense`
- `diplomacyBias`, `logisticsBias`, `strategicBias`

## Economy / logistics / industry
- `economyModifiers.resource_gather` (`Food/Wood/Metal/Wealth/Knowledge/Oil`)
- `logisticsModifiers` (`road_bonus`, `rail_bonus`, `supply_bonus`, `trade_route_bonus`, `tunnel_extraction_bonus`)
- `industryModifiers.factory_throughput_bonus`
- `industryModifiers.refined_goods` (`steel/fuel/munitions/machine_parts/electronics`)

## Military and production
- `unitBonuses.attackMult|hpMult|costMult|trainTimeMult`
- `buildingBonuses.costMult|buildTimeMult|hpMult|trickleMult`

## Doctrine and AI
- `aiDoctrineModifiers`
- `diplomacyDoctrine.aggression_bias|alliance_bias|trade_bias|world_tension_response_bias`
- `diplomacyDoctrine.operation_preference_weights`
- `diplomacyDoctrine.doctrine_tags`

## Unique content
- `uniqueUnits` (unit family -> resolved unit definition ID)
- `uniqueBuildings` (building family -> resolved building definition ID)

Scenario players reference civilization by `players[].civilization`.
