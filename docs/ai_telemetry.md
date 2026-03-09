# AI Telemetry (Deterministic)

This PR adds deterministic counters for AI behavior quality and personality pressure tracking.

## New authoritative counters
- `aiExpansionEarlyCount`
- `aiExpansionRegionalCount`
- `aiExpansionIndustrialCount`
- `aiExpansionCrisisCount`
- `aiExpansionArmageddonCount`
- `aiCounterResponseEvents`
- `aiRailUsageEvents`
- `aiLogisticsDisruptedFronts`
- `aiIndustrialActivationCount`
- `aiIndustrialActivationTick`
- `aiDeterrencePostureChanges`
- `aiOperationLaunches`

These fields are:
1. deterministic,
2. part of authoritative state hash,
3. mirrored into `SimulationStats` for smoke/perf inspection.

## Intended smoke interpretation
- Expansion counters should increase across early/regional/industrial windows instead of stalling in opening.
- Counter response events should rise in mixed-composition scenarios.
- Rail usage and disrupted-front counters should increase in logistics-heavy scenarios.
- Industrial activation should happen in industrial scenarios with non-zero activation tick.
- Deterrence posture/operation counters should rise during crisis/Armageddon pressure windows.
