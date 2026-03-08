# Visual Feedback System (Deterministic, Presentation-Only)

## Scope
This layer provides first-pass deterministic visual feedback for:
- combat impact/weapon exchange
- strategic strikes, interception context, deterrence alerts, Armageddon pulse
- crisis/guardian/objective event emphasis
- industrial/logistics/rail activity
- selection/interaction feedback

## Deterministic reconstruction model
- input: authoritative world state and event ordering only
- no gameplay writes from feedback pass
- no transient feedback serialization in save/load/replay
- effect phase variation derived from `world.tick + stable ids`

## Debug/validation
Debug panel counters:
- COMBAT_EFFECT_SPAWNS
- STRATEGIC_EFFECT_SPAWNS
- CRISIS_EFFECT_SPAWNS
- GUARDIAN_EFFECT_SPAWNS
- INDUSTRY_ACTIVITY_EFFECTS
- SELECTION_FEEDBACK_EVENTS
- FEEDBACK_FALLBACK_COUNT

Validation command set:
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 1800 --dump-hash
./build/rts --headless --scenario scenarios/theater_operations_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 1600 --dump-hash
./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```
