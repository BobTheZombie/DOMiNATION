# Balance Telemetry (Headless)

Headless runs now emit `MATCH_PACING_TELEMETRY` with pacing markers:

- `firstExpansionTick`
- `firstCombatTick`
- `firstFactoryTick`
- `firstRailHubTick`
- `firstStrategicCapabilityTick`
- `firstStrategicLaunchTick`
- per-phase first seen ticks (`phaseEarlyTick`..`phaseArmageddonTick`)
- `finalPhase`
- economy/logistics/pressure indicators (`industrialThroughput`, `railThroughput`, `outOfSupplyUnits`)
- escalation pressure indicators (`strategicWarnings`, `strategicStrikes`)
- end-state path (`matchCondition`)

These values are printed in deterministic headless execution, making them suitable
for regression checks and balance iteration.
