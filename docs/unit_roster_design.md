# Unit Roster Design (Phase-Aligned)

This roster pass formalizes the military taxonomy for the current unit set and aligns production timing with match-flow phases.

## Role taxonomy

- Worker / Engineer: `Worker`
- Line infantry: `Infantry`
- Ranged infantry: `Archer`
- Fast raider / anti-armor: `Cavalry`
- Artillery / siege: `Siege`
- Naval escort: `LightWarship`
- Capital ship: `HeavyWarship`
- Naval siege: `BombardShip`
- Transport: `TransportShip`
- Fighter / interceptor: `Fighter`, `Interceptor`
- Bomber / strike aircraft: `Bomber`, `StrategicBomber`
- Drone / precision support: `ReconDrone`, `StrikeDrone`
- Strategic support missile platform: `TacticalMissile`, `StrategicMissile`

## Phase progression

Production is now gated by `unit_phase_requirement(UnitType)`:

- Early Expansion: economy, line units, scouts/recon, light naval projection.
- Regional Contest: artillery, heavier naval line, and anti-air entry.
- Industrial Escalation: bomber/strike-drone/missile escalation.
- Strategic Crisis+: strategic bomber and strategic missile access.

This keeps early rosters readable and shifts complexity into later phases intentionally.
