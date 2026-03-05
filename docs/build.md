# Build (Linux)

Dependencies (Ubuntu example):
- `build-essential cmake ninja-build libsdl2-dev libglm-dev nlohmann-json3-dev mesa-common-dev libgl1-mesa-dev`

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
