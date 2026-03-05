# Build (Linux)

Dependencies (Ubuntu example):
- `build-essential cmake ninja-build libsdl2-dev libglm-dev nlohmann-json3-dev libgl1-mesa-dev`

Build:
```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
./rts
```
