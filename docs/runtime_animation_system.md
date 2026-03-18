# Runtime Animation System

The runtime animation system is a bounded presentation-only layer integrated with model rendering.

## Deterministic rules
- Animation never changes authoritative gameplay state or simulation hash.
- Playback is resolved from `(stable entity/building/object id + presentation tick + resolved style animation mapping + model clip metadata)`.
- Missing or invalid clip references deterministically fall back to either the first available clip or static rendering.

## Supported bounded states
- `idle`, `move`, `attack`, `work`, `warning`, `aura`, plus optional posture hints (`selected`, `low_supply`, `damaged`).
- Loop and one-shot hints are authored declaratively in stylesheet JSON (`animation.playback_hints`).

## Debug counters
- `ANIMATION_RESOLVE_COUNT`
- `ANIMATION_FALLBACK_COUNT`
- `ACTIVE_ANIMATED_INSTANCES`
- `CLIP_PLAY_EVENTS`
- `LOOPING_CLIP_INSTANCES`
