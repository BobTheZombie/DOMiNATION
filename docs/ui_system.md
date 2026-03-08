# UI System (HUD/Panel Polish)

## Layout model
The polished HUD is divided into four deterministic presentation regions:
1. Top strategy bar (economy, civ, age, population, strategic status)
2. Strategic side column (alerts, objectives, crisis/event feed)
3. Bottom command deck (selection/context + mission message history)
4. Minimap corner frame

## Shared visual language
`engine/ui/ui_theme.*` provides reusable ImGui helpers:
- spacing/padding/window rounding tuning
- civ accent color generation
- semantic state colors (warning/success/failure/info)
- section header and state text helpers

## Panel hierarchy
- Core HUD: `engine/ui/hud.cpp`
- Production UX: `engine/ui/production_menu.cpp`
- Research UX: `engine/ui/research_panel.cpp`
- Diplomacy + Operations UX: `engine/ui/diplomacy_panel.cpp`
- Debug-only surfaces: `engine/debug/debug_panels.cpp`

## Strategic alert presentation rules
- Highlight Armageddon and high world-tension states in warning/failure colors.
- Keep objective/event ordering simulation-driven.
- Present mission/objective transitions with visible state emphasis.

## Minimap and selection presentation rules
- Minimap stays renderer-owned; HUD provides framed context and readable strategic marker summary.
- Selection card emphasizes identity, owner, health/status, and content marker summary.

## Deterministic validation commands
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 600 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 1800 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```


## Visual feedback debug controls

Debug Visualization panel now includes toggles for deterministic visual feedback enable/disable and a source overlay debug frame. Counters are displayed per category to validate coverage and fallback behavior.


## Front-end shell integration
The runtime now includes a player-facing title/setup shell before gameplay starts.

- Main menu: Skirmish, Scenario, Campaign, Load Game, Options, Quit
- Setup cards emphasize player slots, civ selection, world settings, victory settings, and launch summary validation
- Scenario/campaign/save browsers surface metadata from authored files with deterministic fallbacks
- Debug panels remain available after match entry and are visually separated from player setup flow

