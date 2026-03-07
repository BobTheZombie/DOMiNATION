# Industrial Economy System

The industrial layer adds deterministic refined goods production on top of base resources.

## Refined goods
- steel
- fuel
- munitions
- machine_parts
- electronics

Recipes are data-driven from `content/industrial_recipes.json` and can optionally be scenario-overridden via `industrialRecipes`.

## Factories
Factory building types:
- SteelMill
- Refinery
- MunitionsPlant
- MachineWorks
- ElectronicsLab
- FactoryHub

Each factory stores authoritative state:
- `recipeIndex`
- `cycleProgress`
- `inputBuffer`
- `outputBuffer`
- `paused` / `blocked` / `active`
- `throughputBonus`

## Throughput and logistics
Factories run in tick-driven deterministic cycles. Throughput bonuses can come from nearby roads, rail nodes, ports, and FactoryHub buildings.

## Gameplay integration
Representative refined-good requirements are wired for:
- cavalry (steel + fuel)
- bombers (fuel + machine_parts)
- missiles (munitions + electronics)
- drones (electronics)

## Determinism + authority
Refined goods and factory states are authoritative and included in save/load and state hashing.
