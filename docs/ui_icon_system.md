# UI Icon System

## Deterministic resolution model
1. Exact content icon ID (unit/building/event/mission message).
2. Civ/theme mapping (emblems and civ-specific icon affinity).
3. Category mapping (resource/unit/building/objective/event/warning/guardian).
4. Stable fallback icon ID.

## Counters
- `ICON_RESOLVE_COUNT`
- `MARKER_RESOLVE_COUNT`
- `ALERT_RESOLVE_COUNT`
- `PRESENTATION_FALLBACK_COUNT`

These are debug-only presentation counters and do not alter authoritative simulation state.
