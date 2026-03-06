# DOMiNATION

Linux-first original RTS vertical slice inspired by classic nation-building RTS gameplay loops.

## Locked stack
- C++20
- CMake + Ninja
- SDL2
- OpenGL
- GLM
- JSON content files

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
- **F1/F2/F3**: territory / border / fog overlays
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
  - `F9` toggle editor, `Tab` cycle tool, `O` change owner, LMB place/remove, `F5` save scenario JSON (`--editor-save <file>`).

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
