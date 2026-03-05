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
./build/rts --headless --smoke --ticks 600 --seed 1234 --dump-hash
```

## CLI flags
- `--headless` run simulation without SDL window or GL context
- `--smoke` enable deterministic validation checks and strict failures
- `--ticks <N>` run fixed number of sim ticks and exit
- `--seed <S>` deterministic world seed
- `--map-size <W>x<H>` override map dimensions
- `--dump-hash` print deterministic map/state hashes

## Controls
- **WASD**: pan camera in RTS mode
- **Mouse wheel**: zoom
- **Left click**: select a unit
- **Right click**: move selected unit
- **G**: toggle GOD Mode (full reveal + high zoom cap)
- **1**: toggle territory overlay
- **2**: toggle border overlay
- **3**: toggle fog overlay (forced OFF while in GOD Mode)
