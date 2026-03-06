# Build (Linux)

Dependencies (Ubuntu example):
- `build-essential cmake ninja-build libsdl2-dev libglm-dev nlohmann-json3-dev mesa-common-dev libgl1-mesa-dev pkg-config`

SDL2 detection order in CMake:
1. `find_package(SDL2 CONFIG QUIET)`
2. `find_package(SDL2 MODULE QUIET)` (CMake FindSDL2 module)
3. Fallback to `pkg-config` (`sdl2.pc`) and expose compatible target `SDL2::SDL2`

This allows standard Ubuntu `libsdl2-dev` installations (which often omit `SDL2Config.cmake`) and environments that only provide CMake's FindSDL2 module to build without extra setup.

Build:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run interactive (desktop required):
```bash
./build/rts
```

Run headless deterministic smoke (no display server required):
```bash
./build/rts --headless --smoke --ticks 600 --seed 1234 --dump-hash
```

Additional examples:
```bash
./build/rts --headless --ticks 1200 --seed 99
./build/rts --headless --smoke --map-size 192x192 --ticks 400 --seed 777
```
