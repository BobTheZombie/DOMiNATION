# DOMiNATION

Linux-first original RTS vertical slice inspired by classic nation-building RTS gameplay loops.

## Locked stack
- C++20
- CMake + Ninja
- SDL2
- OpenGL
- GLM
- JSON content files

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

## Controls
- **WASD**: pan camera
- **Mouse wheel**: zoom
- **Left click**: select unit / confirm build placement
- **Right click**: move selected unit / cancel build placement
- **G**: toggle GOD Mode (full reveal + high zoom cap)
- **B**: toggle build menu (`1..7` pick building: House/Farm/Lumber/Mine/Market/Library/Barracks)
- **T**: toggle train menu (`1` Worker at City Center, `2` Infantry at Barracks, `Backspace` cancel queue front)
- **R**: toggle research panel (`1` Age Up)
- **Esc**: cancel active build placement
- **1/2/3**: territory / border / fog overlays
