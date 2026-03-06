# Rendering

- Supported target resolutions: 1920x1080, 2560x1440, 3840x2160.
- Window resize path handles `SDL_WINDOWEVENT_SIZE_CHANGED` by updating viewport, projection, picking math, and UI anchors.
- CLI controls: `--width`, `--height`, `--fullscreen`, `--borderless`, `--render-scale`, `--ui-scale`.
- Render scaling uses offscreen framebuffer rendering then blits/upscales to display resolution.
- HUD and debug overlays scale with `--ui-scale` for 4K readability.
