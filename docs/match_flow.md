# Match Flow Phases

The simulation now classifies the live match into deterministic flow phases:

1. `EARLY_EXPANSION`
2. `REGIONAL_CONTEST`
3. `INDUSTRIAL_ESCALATION`
4. `STRATEGIC_CRISIS`
5. `ARMAGEDDON_ENDGAME`

Phase computation is derived from authoritative state (age mix, military footprint,
industrial/logistics throughput, world tension, strategic readiness, and Armageddon state).

## Design intent

- Early phase emphasizes worker growth and first infrastructure.
- Regional contest emphasizes first army pressure and local wars.
- Industrial escalation raises relevance of factories/rail tempo.
- Strategic crisis tightens endgame pressure around deterrence and decisive victory paths.
- Armageddon remains last-man-standing and disables score/wonder resolution.

## Determinism

Phase state (`matchFlowPhase`, `matchFlowPhaseTick`) is included in world state and hash,
so identical seeds/scenarios/commands/ticks/thread counts produce identical outcomes.
