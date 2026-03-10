# Smoke Validation Contract

Smoke mode (`--smoke`) is deterministic validation, not "always-force-combat".

## Scenario validation intent

`engine/platform/app.cpp` classifies smoke intent by scenario filename and/or mission `scenarioTags`:

- `combat` (default)
- `logistics`
- `crisis` / `world_events`
- `guardians`
- `industry`
- `strategic_escalation`

Some scenarios can opt into combat assertions explicitly with a `combat` tag.

## Intent-gated assertions

All smoke runs still enforce core deterministic checks (finite state, deterministic hash output, save/load parity checks, replay/hash behavior, and thread parity workflows).

Intent-specific assertions then apply:

- **Combat**: military pacing telemetry must activate (`firstCombatTick` or combat engagements).
- **Logistics**: rail/logistics capability and throughput must activate (no forced combat pacing).
- **Crises**: world event trigger/active-or-resolved flow must activate (no forced combat pacing).
- **Guardians**: guardian sites plus discover/spawn lifecycle must activate (no forced combat pacing).
- **Industry**: factory/industrial pacing must activate.
- **Strategic escalation**: strategic warning/strike activity must activate.

## Required scenario outcomes

- `rail_logistics_test` validates logistics, not military pacing.
- `world_events_test` validates event/crisis flow, not military pacing.
- `mythic_guardians_multi_test` validates guardian behavior, not military pacing.
- Combat-focused scenarios continue requiring military pacing telemetry.
