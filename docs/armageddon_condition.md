# Armageddon Condition

Armageddon is an authoritative deterministic global escalation state.

Default trigger (data-driven in scenarios):
- `armageddonNationsThreshold = 2`
- `armageddonUsesPerNationThreshold = 2`
- A qualifying use is a launched `StrategicMissile`, `StrategicBomberStrike`, or `AbstractWMD`.

When active:
- `armageddonActive = true`
- `armageddonTriggerTick` set
- `lastManStandingModeActive = true`
- world tension forced to max
- score/wonder victory disabled
- match resolves by last civilization standing
- bounded penalties: food + industry disruption factors

Serialized fields:
- `armageddonActive`, `armageddonTriggerTick`, `lastManStandingModeActive`
- `armageddonNationsThreshold`, `armageddonUsesPerNationThreshold`
- `nuclearUseCountTotal`, `nuclearUseCountByPlayer[]`
