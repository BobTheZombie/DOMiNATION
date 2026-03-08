# Game Setup & Match Flow Shell

This PR adds a player-facing setup shell in the SDL/ImGui runtime (`engine/platform/app.cpp`) without changing the authoritative simulation path.

## Front-end screens
- **Main Menu**: Skirmish, Scenario, Campaign, Load Game, Options, Quit.
- **Skirmish Setup**: player slots, civ selection, world preset, map size, seed, Armageddon thresholds, world events/guardian toggles, victory toggles, summary + validation.
- **Scenario Browser**: scans `scenarios/*.json`, shows title/description/civ/world/difficulty/briefing metadata where present.
- **Campaign Browser**: scans `campaigns/*.json`, shows campaign metadata and first mission briefing metadata.
- **Load Game**: scans `saves/*.json` and loads selected save with metadata preview.
- **Options**: lightweight player-facing display/UI scale controls.

## Deterministic mapping rules
Setup values only map to existing authoritative runtime fields:
- seed/map/preset map to `World` generation settings
- civ choices map to existing `civilization_runtime_for(...)` assignment
- victory toggles map to `world.config.allowConquest/allowScore/allowWonder`
- Armageddon controls map to existing `world.armageddon*` thresholds
- scenario/campaign/save launch reuses existing scenario/campaign/load codepaths

No transient menu state is serialized into authoritative save state.

## Validation + fallback behavior
- Prevent launch when player-slot or victory configuration is invalid.
- Missing scenario/campaign/save lists are non-fatal with clear placeholder text.
- Metadata fields fall back cleanly to file-derived defaults when missing.

## Deterministic smoke commands
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --seed 1234 --world-preset continents --smoke --ticks 400 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 800 --dump-hash
./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 600 --save /tmp/setup_flow_save.json --dump-hash
./build/rts --headless --load /tmp/setup_flow_save.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```
