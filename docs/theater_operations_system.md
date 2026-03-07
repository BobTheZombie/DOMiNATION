# Theater Operations System

The theater operations subsystem provides deterministic large-scale command orchestration for AI-driven warfare.

## Data model

Authoritative runtime structures:

- `TheaterCommand`
  - `theaterId`, `owner`, `bounds`, `priority`
  - `activeOperations`
  - assigned forces (`assignedArmyGroups`, `assignedNavalTaskForces`, `assignedAirWings`)
  - `supplyStatus`, `threatLevel`
- `ArmyGroup`
  - deterministic list of `unitIds`
  - `stance` (`Offensive`/`Defensive`)
  - `assignedObjective`
- `NavalTaskForce`
  - deterministic fleet composition (`unitIds`)
  - mission (`Patrol`/`Escort`/`Assault`)
  - `assignedObjective`
- `AirWing`
  - deterministic squadron membership (`squadronIds`)
  - mission (`Bombing`/`Interception`)
  - `assignedObjective`
- `OperationalObjective`
  - `objectiveType`
  - `targetRegion`
  - `requiredForce`
  - `startTick`, `durationTicks`
  - `outcome` (`InProgress`/`Success`/`Failure`)

## Deterministic planning

Every 80 ticks, CPU players perform theater planning:

1. Divide map into two deterministic theater bands (north/south).
2. Build formation pools (ground/naval/air) by stable unit iteration order.
3. Select objective type based on doctrine (`aggression`, `defense`, `logisticsBias`, `aiNavalPriority`, `aiAirPriority`, `aiStrategicPriority`).
4. Assign formations to objective and theater by deterministic split.
5. Evaluate outcome with deterministic supply/threat heuristics.

No random numbers or thread-order dependent data is used in these decisions.

## Serialization and replay

Theater command structures are serialized in scenario/save payloads and included in authoritative hashing to keep:

- save/load parity
- replay determinism
- thread-count parity (`--threads 1/4/8`)

## UI and debug

- Operations panel now shows theater summaries, active operational objectives, and assigned force counts.
- Debug panel reports theater/operation counters and formation totals.

## Smoke scenario

`scenarios/theater_operations_test.json` validates:

- multi-region map setup
- multiple CPU factions
- ground/naval/air composition for theater assignment
- operational objective execution and outcome recording

Run:

```bash
./build/rts --headless --scenario scenarios/theater_operations_test.json --smoke --ticks 3000 --dump-hash
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 8 --hash-only
```
