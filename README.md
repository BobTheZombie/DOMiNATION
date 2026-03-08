# DOMiNATION

Linux-first original RTS vertical slice inspired by classic nation-building RTS gameplay loops.

## Locked stack
- C++20
- CMake + Ninja
- SDL2
- OpenGL
- GLM
- JSON content files


## Dear ImGui integration
- ImGui is vendored as a pinned git submodule at `third_party/imgui` (official `ocornut/imgui`).
- Bootstrap after fresh clone:
  - `git submodule update --init --recursive`
- CMake builds ImGui internally by default (`RTS_ENABLE_IMGUI=ON`) with SDL2 + OpenGL3 backends.
- Disable only when needed: `-DRTS_ENABLE_IMGUI=OFF`.
- To update/pin: checkout desired commit in `third_party/imgui`, commit the submodule pointer update.

## Asset pipeline runtime ingestion
Required runtime manifests in `content/`:
- `asset_manifest.json`
- `atlas_manifest.json`
- `biome_manifest.json`
- `civilization_theme_manifest.json`
- `lod_manifest.json`

Commands:
```bash
python tools/asset_pipeline/build_atlases.py
python tools/asset_pipeline/package_assets.py
python tools/validate_content_pipeline.py
```

Asset browser (ImGui):
- Press `F10` in-game to toggle the Asset Browser panel.
## Build dependencies (Ubuntu)
```bash
sudo apt-get update
sudo apt-get install -y \
  libsdl2-dev pkg-config libglm-dev nlohmann-json3-dev mesa-common-dev libgl1-mesa-dev ninja-build cmake g++
```

SDL2 is discovered in this order: `find_package(SDL2 CONFIG)`, then CMake's `FindSDL2` module, then `pkg-config` (`sdl2.pc`). This covers common Ubuntu/Debian setups that may omit `SDL2Config.cmake`.

## Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run
Interactive (requires desktop session / X11 / Wayland):
```bash
./build/rts
```

Headless deterministic smoke mode (CI/container friendly):
```bash
./build/rts --headless --smoke --ticks 1200 --seed 1234 --dump-hash
```

## CLI flags
- `--headless` run simulation without SDL window or GL context
- `--smoke` enable deterministic validation checks and strict failures
- `--ticks <N>` run fixed number of sim ticks and exit
- `--seed <S>` deterministic world seed
- `--world-preset <pangaea|continents|archipelago|inland_sea|mountain_world>` deterministic macro world shape preset
- `--map-size <W>x<H>` override map dimensions
- `--dump-hash` print deterministic map/state hashes
- `--nav-debug` print navigation diagnostics counters in headless dump output
- `--flow-visualize` request flow-field overlay in interactive mode (currently routed to debug path)
- `--ai-attack-early` lower AI attack threshold for fast testing
- `--ai-aggressive` reduce AI retreat sensitivity
- `--combat-debug` print combat smoke diagnostics (engagements/switches/retreats/damage)
- `--time-limit-ticks <N>` set score-victory time limit in ticks
- `--record-replay <file>` record authoritative command replay JSON
- `--replay <file>` play replay JSON deterministically
- `--replay-verify` compare replay final hash against recorded expected hash
- `--replay-stop-tick <N>` stop replay run at absolute tick `N`
- `--replay-speed <multiplier>` replay speed multiplier (interactive pacing)
- `--replay-summary-only` run replay and emit summary/hash output only
- `--save <file>` save authoritative game state JSON
- `--load <file>` load authoritative game state JSON
- `--autosave-tick <N>` autosave when tick reaches `N`
- `--force-score-victory` disable wonder win pressure for score-smoke helper
- `--force-wonder-progress` helper flag reserved for wonder smoke shaping
- `--match-debug` print match-flow diagnostics
- `--threads <N>` set deterministic worker pool size for job graph execution
- `--hash-only` print only final hash line (plus required diagnostics/errors)


## World generation smoke checks
```bash
./build/rts --headless --smoke --ticks 200 --seed 1234 --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset archipelago --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset mountain_world --dump-hash
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 1 --hash-only
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 4 --hash-only
./build/rts --headless --smoke --ticks 200 --seed 1234 --world-preset continents --threads 8 --hash-only
```

## Controls
- **WASD**: pan camera
- **Mouse wheel**: zoom
- **Left click**: select unit / confirm build placement
- **Right click**: move selected unit / cancel build placement
- **G**: toggle GOD Mode (full reveal + high zoom cap)
- **B**: toggle build menu (`1..7` pick building: House/Farm/Lumber/Mine/Market/Library/Barracks)
- **T**: toggle train menu (`1` Worker at City Center, `2` Infantry at Barracks, `Backspace` cancel queue front)
- **R**: toggle research panel (`1` Age Up); in replay mode restart replay
- **Esc**: cancel active build placement
- **Ctrl+1..9**: assign control group
- **1..9**: select control group (double tap focuses camera on group)
- **M**: toggle minimap visibility
- **F1**: toggle HUD/debug panel visibility
- **F2**: toggle production panel
- **F3**: toggle research panel
- **F4**: toggle diplomacy panel
- **F5**: toggle operations panel
- **Replay mode controls**: `Space` pause/resume, `Right` step forward (paused), `Left` rewind fallback (restart+seek), `[`/`]` jump ticks, `+`/`-` speed

## Victory conditions
- **Conquest**: eliminate all enemy capitals/city centers.
- **Score**: when `time-limit-ticks` is reached, highest deterministic score wins; ties resolve to lowest player ID.
- **Wonder**: complete and hold a Wonder for configured hold ticks.

## Replay
- Record: `./build/rts --headless --smoke --ticks 2200 --seed 1234 --time-limit-ticks 1800 --record-replay /tmp/test_replay.json --dump-hash`
- Playback+verify: `./build/rts --headless --replay /tmp/test_replay.json --replay-verify`

## Save / load
- Save during headless smoke: `./build/rts --headless --smoke --ticks 1400 --seed 1234 --autosave-tick 900 --save /tmp/state.json --dump-hash`
- Continue from save: `./build/rts --headless --load /tmp/state.json --smoke --ticks 1400 --dump-hash`

Headless output adds `SAVE_RESULT` / `LOAD_RESULT` / `REPLAY_RESULT` lines for automation.

## Scenarios and editor
- Launch authored scenario: `./build/rts --scenario scenarios/test_scenario.json`
- List shipped scenarios: `./build/rts --list-scenarios`
- Headless scenario smoke: `./build/rts --headless --scenario scenarios/test_scenario.json --smoke --ticks 1200 --dump-hash`
- Trigger smoke: `./build/rts --headless --scenario scenarios/trigger_test.json --smoke --ticks 1800 --dump-hash`
- Editor mode: `./build/rts --editor --scenario scenarios/test_scenario.json`
  - `F9` toggle editor, `Tab` cycle tool, `O` change owner, LMB place/remove.
  - Scenario Editor ImGui panel supports Save / Save As / Load for round-trip authoring under `scenarios/*.json`.
  - Unsupported authored fields are explicitly noted in the editor panel instead of silently dropped.

Objectives/triggers are simulation-authoritative and visible via HUD/objective log overlays.


## Resolution and display flags
- `--width <N> --height <N>`
- `--fullscreen`
- `--borderless`
- `--render-scale <float>` (0.5-1.0)
- `--ui-scale <float>`

Examples:
- `./build/rts --width 1920 --height 1080`
- `./build/rts --width 2560 --height 1440 --render-scale 0.75`
- `./build/rts --width 3840 --height 2160 --ui-scale 1.5`

Civilization data: `content/civilizations.json`.


Headless perf mode also emits deterministic `EVENT_COUNT` lines sourced from the gameplay event stream.

## Diplomacy / tension / espionage

- Authoritative diplomacy matrix now controls bilateral `Allied`, `Neutral`, `War`, and `Ceasefire` relation state.
- Authoritative treaties support alliance, trade agreement, and open-borders style access.
- Global authoritative `worldTension` escalates from war/espionage/elimination outcomes and influences AI diplomatic posture.
- Authoritative espionage operations execute deterministic timed effects (`RECON_CITY`, `REVEAL_ROUTE`, `SABOTAGE_ECONOMY`, `SABOTAGE_SUPPLY`, `COUNTERINTEL`).
- AI exposes deterministic posture labels: `EXPANSIONIST`, `DEFENSIVE`, `TRADE_FOCUSED`, `ESCALATING`, `TOTAL_WAR`.

Additional perf counters:
- `WORLD_TENSION`
- `ALLIANCE_COUNT`
- `WAR_COUNT`
- `ACTIVE_ESPIONAGE_OPS`
- `POSTURE_CHANGES`
- `DIPLOMACY_EVENTS`

Deterministic diplomacy smoke commands:

```bash
./build/rts --headless --ticks 1800 --seed 1234 --dump-hash
./build/rts --headless --scenario scenarios/diplomacy_test.json --ticks 1800 --dump-hash
./build/rts --headless --scenario scenarios/diplomacy_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/diplomacy_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/diplomacy_test.json --threads 8 --hash-only
./build/rts --headless --scenario scenarios/diplomacy_test.json --threads 8 --ticks 2400 --dump-hash --smoke
```

## Deterministic threaded simulation validation

Chunked deterministic simulation now parallelizes movement integration, fog updates, territory updates, and async navigation generation while committing authoritative state in stable order.

Validation commands:

```bash
./build/rts --headless --spawn-army 500 --cpu-only-battle --threads 1 --ticks 1200 --dump-hash
./build/rts --headless --spawn-army 500 --cpu-only-battle --threads 4 --ticks 1200 --dump-hash
./build/rts --headless --spawn-army 500 --cpu-only-battle --threads 8 --ticks 1200 --dump-hash
./build/rts --headless --spawn-army 1000 --cpu-only-battle --threads 8 --ticks 600 --dump-hash
```

Expected outcome:
- identical final hash for thread counts 1/4/8 for same seed/inputs/ticks,
- no deadlock/crash,
- combat and deaths observed,
- perf output includes: `THREADS`, `JOB_COUNT`, `CHUNK_COUNT`, `MOVEMENT_TASKS`, `FOG_TASKS`, `TERRITORY_TASKS`, `NAV_REQUESTS`, `NAV_COMPLETIONS`, `NAV_STALE_DROPS`, `EVENT_COUNT`.


## Logistics smoke checks

```bash
./build/rts --headless --ticks 1600 --seed 1234 --dump-hash
./build/rts --headless --spawn-army 500 --cpu-only-battle --threads 8 --ticks 1200 --dump-hash
./build/rts --headless --ticks 1600 --threads 1 --hash-only
./build/rts --headless --ticks 1600 --threads 4 --hash-only
./build/rts --headless --ticks 1600 --threads 8 --hash-only
./build/rts --headless --scenario scenarios/logistics_test.json --ticks 1400 --dump-hash
```

Perf counters now include `ROAD_COUNT`, `ACTIVE_TRADE_ROUTES`, `SUPPLIED_UNITS`, `LOW_SUPPLY_UNITS`, `OUT_OF_SUPPLY_UNITS`, `OPERATION_COUNT`.


### Naval / Coast deterministic smoke
- Naval baseline: `./build/rts --headless --ticks 1800 --seed 1234 --dump-hash`
- Coastal scenario: `./build/rts --headless --scenario scenarios/naval_test.json --ticks 1800 --dump-hash`
- Thread parity: `./build/rts --headless --scenario scenarios/naval_test.json --threads 1 --hash-only` and `--threads 4/8`
- Amphibious stress: `./build/rts --headless --scenario scenarios/naval_test.json --threads 8 --ticks 2200 --dump-hash`


## UI authoritative command flow
- Production/research/diplomacy actions in ImGui call authoritative simulation commands (`enqueue_train_unit`, `cancel_queue_item`, `enqueue_age_research`, `declare_war`, `form_alliance`, `establish_trade_agreement`).
- UI panels are presentation-only and read state from authoritative `World` data each frame.
- Event Log + Notifications consume deterministic gameplay events stream.
- Command History panel records recently issued authoritative commands for debugging and smoke verification.

## Strategic warfare smoke checks
- `./build/rts --headless --smoke --ticks 2200 --seed 1234 --dump-hash`
- `./build/rts --headless --smoke --ticks 2200 --seed 1234 --threads 1 --hash-only`
- `./build/rts --headless --smoke --ticks 2200 --seed 1234 --threads 4 --hash-only`
- `./build/rts --headless --smoke --ticks 2200 --seed 1234 --threads 8 --hash-only`
- `./build/rts --headless --scenario scenarios/strategic_warfare_test.json --smoke --ticks 2600 --dump-hash`

## Campaign scenario scripting

This branch adds deterministic mission objectives, trigger-action authored scripting, mission runtime state, and sandboxed Lua hooks for scenario orchestration.

Smoke scenario: `scenarios/campaign_test.json` with optional script `scripts/campaign_test.lua`.

## Civilization differentiation and deterministic save/load

- Civilization runtime now supports economy/military/science plus diplomacy/logistics/strategic biases, doctrine modifiers, and per-family unique unit/building definition IDs loaded from `content/civilizations.json`.
- Civilization identity layer now includes identity metadata, economy/logistics/industry modifiers, diplomacy/theater operation preferences, unique unit+building replacements, and deterministic civ differentiation counters.
- Unique replacements are deterministic and replay/save-safe via authoritative `definitionId` persisted on spawned units/buildings.
- Campaign mission/objective/trigger runtime fields used by authority are serialized in save files to preserve mid-mission parity.
- Smoke commands:
  - `./build/rts --headless --scenario scenarios/campaign_test.json --smoke --ticks 1200 --save /tmp/campaign_save.json --dump-hash`
  - `./build/rts --headless --load /tmp/campaign_save.json --smoke --ticks 2200 --dump-hash`
  - `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 2200 --dump-hash`
  - `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 2600 --dump-hash`
  - `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 3000 --dump-hash`
  - `./build/rts --headless --scenario scenarios/civ_test.json --threads 1 --hash-only`
  - `./build/rts --headless --scenario scenarios/civ_test.json --threads 4 --hash-only`
  - `./build/rts --headless --scenario scenarios/civ_test.json --threads 8 --hash-only`
  - `./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 1500 --save /tmp/civ_save.json --dump-hash`
  - `./build/rts --headless --load /tmp/civ_save.json --smoke --ticks 3000 --dump-hash`

- Mountain extraction + underground tunnel network layer (deterministic node/edge model, snow-capped mountain generation, deep deposits).

## Mythic Guardians (new)

The engine now includes a reusable deterministic Mythic Guardian layer:

- Data-driven definitions in `content/mythic_guardians.json`
- Deterministic procedural + scenario-authored guardian sites
- Full guardian suite: **Snow Yeti**, **Kraken**, **Sandworm**, and **Forest Spirit**
- Save/load/replay-authoritative guardian state
- Basic AI/UI/editor/scenario integration

Reference docs: `docs/mythic_guardians.md`.

- Mythic guardian deterministic smoke commands:
  - `./build/rts --headless --smoke --ticks 1800 --seed 1234 --dump-hash`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_test.json --smoke --ticks 2200 --dump-hash`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 2400 --dump-hash`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 1 --hash-only`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 4 --hash-only`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 8 --hash-only`
  - `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 1200 --save /tmp/guardian_save.json --dump-hash`
  - `./build/rts --headless --load /tmp/guardian_save.json --smoke --ticks 2400 --dump-hash`


## Industrial rail logistics (deterministic)

This build includes authoritative railroad logistics:
- deterministic rail nodes/edges/networks
- deterministic supply/freight train state
- rail throughput/disruption counters in PERF output
- scenario support: `railNodes`, `railEdges`, `railNetworks`, `trains`

Smoke commands:
- `./build/rts --headless --smoke --ticks 2200 --seed 1234 --dump-hash`
- `./build/rts --headless --scenario scenarios/rail_logistics_test.json --smoke --ticks 2600 --dump-hash`
- `./build/rts --headless --scenario scenarios/rail_logistics_test.json --threads 1 --hash-only`
- `./build/rts --headless --scenario scenarios/rail_logistics_test.json --threads 4 --hash-only`
- `./build/rts --headless --scenario scenarios/rail_logistics_test.json --threads 8 --hash-only`
- `./build/rts --headless --scenario scenarios/rail_logistics_test.json --smoke --ticks 1400 --save /tmp/rail_save.json --dump-hash`
- `./build/rts --headless --load /tmp/rail_save.json --smoke --ticks 2600 --dump-hash`

## Campaign progression

Use `--campaign campaigns/test_campaign.json` for deterministic multi-mission progression with carryover/branching and campaign save-load support.

## Industrial economy smoke checks
```bash
./build/rts --headless --smoke --ticks 2600 --seed 1234 --dump-hash
./build/rts --headless --scenario scenarios/industrial_economy_test.json --smoke --ticks 3000 --dump-hash
./build/rts --headless --scenario scenarios/industrial_economy_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/industrial_economy_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/industrial_economy_test.json --threads 8 --hash-only
./build/rts --headless --scenario scenarios/industrial_economy_test.json --smoke --ticks 1600 --save /tmp/industrial_save.json --dump-hash
./build/rts --headless --load /tmp/industrial_save.json --smoke --ticks 3000 --dump-hash
```


## Theater operations (deterministic)

This build adds deterministic large-scale command structures:

- theater commands
- army groups, naval task forces, and air wings
- operational objectives (offensive/defensive/encirclement/blockade/strategic strike style goals)
- doctrine-influenced AI operational planning
- ImGui/debug visibility for theaters + operations

Smoke and thread parity:

```bash
./build/rts --headless --scenario scenarios/theater_operations_test.json --smoke --ticks 3000 --dump-hash
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/theater_operations_test.json --threads 8 --hash-only
```


## Strategic deterrence smoke
- `./build/rts --headless --smoke --ticks 3200 --seed 1234 --dump-hash`
- `./build/rts --headless --scenario scenarios/strategic_deterrence_test.json --smoke --ticks 3600 --dump-hash`
- Thread parity: `--threads 1|4|8 --hash-only` against `scenarios/strategic_deterrence_test.json`
- Save/load parity:
  - `./build/rts --headless --scenario scenarios/strategic_deterrence_test.json --smoke --ticks 1800 --save /tmp/deterrence_save.json --dump-hash`
  - `./build/rts --headless --load /tmp/deterrence_save.json --smoke --ticks 3600 --dump-hash`

## Campaign presentation layer
- Added authored campaign briefing/debrief windows, mission message log, campaign progression summary, and objective transition debug panes (ImGui).
- Story metadata is data-driven (`portraitId`, `iconId`, `imageId`, style tags) and non-authoritative; deterministic mission/campaign state remains authoritative.

## Civilization content packs

The engine now resolves civilization-specific unit/building packs authoritatively for Rome, China, Europe, and Middle East. Resolved definition IDs are used by production, AI, scenario placement, save/load, and replay.

Headless validation:

```bash
./build/rts --headless --scenario scenarios/civ_test.json --smoke --ticks 3200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 3600 --dump-hash
```

## Visual/content polish
See `docs/visual_content_polish.md` for deterministic presentation mapping, fallback rules, and smoke commands.

## Civilization Expansion + Armageddon

Added civilization packs: Russia, USA, Japan, EU, UK, Egypt, and Tartaria (explicitly fictional/alt-history).

Armageddon Condition is now authoritative and deterministic: if at least two different civilizations each perform at least two qualifying strategic nuclear uses, the game enters global Armageddon and switches to last-civilization-standing victory mode.

Smoke commands:
- `./build/rts --headless --scenario scenarios/civ_expansion_test.json --smoke --ticks 3200 --dump-hash`
- `./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 3600 --dump-hash`
- `./build/rts --headless --scenario scenarios/armageddon_test.json --threads 1 --hash-only`
- `./build/rts --headless --scenario scenarios/armageddon_test.json --threads 4 --hash-only`
- `./build/rts --headless --scenario scenarios/armageddon_test.json --threads 8 --hash-only`
- `./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 1800 --save /tmp/armageddon_save.json --dump-hash`
- `./build/rts --headless --load /tmp/armageddon_save.json --smoke --ticks 3600 --dump-hash`

## Civilization visual/content pack smoke

Deterministic validation commands:

```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 600 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```

Expected checks:
- deterministic civ asset resolution
- deterministic fallback behavior
- no asset lookup crash
- parity of final authoritative hash across thread counts

## Ideology and Alliance Blocs
Dynamic ideology alignment and deterministic alliance bloc behavior are now supported. See `docs/ideology_bloc_system.md` and `scenarios/bloc_test.json` for format and validation commands.

## UI/UX polish pass (HUD + panel hierarchy)
- Added a structured HUD layout with clear regions: top strategy bar, right-side strategic alerts/objectives feed, bottom command deck, and minimap corner frame (`engine/ui/hud.cpp`).
- Unified panel presentation now flows through shared ImGui theme helpers in `engine/ui/ui_theme.*`.
- Production, Research, Diplomacy, and Operations panels were updated to a consistent visual language and information hierarchy while keeping authoritative simulation logic unchanged.

### Deterministic smoke suite (UI integration)
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

## Terrain presentation layer (first-pass strategy map)

Renderer now resolves deterministic terrain materials from authoritative world data only (biome map, terrain class, elevation, fertility, river/lake/coast markers, mountain biomes, resource/world feature placement).

Debug panel overlays:
- `terrain material overlay`
- `water feature overlay`
- counters: `TERRAIN_MATERIAL_RESOLVES`, `WATER_FEATURE_RESOLVES`, `FOREST_CLUSTER_COUNT`, `MOUNTAIN_FEATURE_COUNT`, `PRESENTATION_FALLBACK_COUNT`

Determinism validation commands:
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --smoke --ticks 400 --seed 1234 --world-preset continents --dump-hash
./build/rts --headless --smoke --ticks 400 --seed 1234 --world-preset archipelago --dump-hash
./build/rts --headless --smoke --ticks 400 --seed 1234 --world-preset mountain_world --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 600 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```
