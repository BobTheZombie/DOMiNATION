# Civilization format

Civilizations are defined in `content/civilizations.json` as an array.

Fields:
- `id`
- `displayName`
- `economyBias`
- `militaryBias`
- `scienceBias`
- `aggression`
- `defense`
- `preferredUnits`
- `uniqueModifiers`

Scenario players reference civilization by `players[].civilization`.
