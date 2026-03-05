# DOMiNATION

Linux-first original RTS vertical slice inspired by classic nation-building RTS gameplay loops.

## Locked stack
- C++20
- CMake + Ninja
- SDL2
- OpenGL
- GLM
- JSON content files

## Build + run
```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
./rts
```

## How to play
- **WASD**: pan camera in RTS mode
- **Mouse wheel**: zoom
- **Left click**: select a unit
- **Right click**: move selected unit
- **G**: toggle GOD Mode (full reveal + higher zoom range)
- Win by score at time limit (10 minutes) or through conquest hooks on capitals.
