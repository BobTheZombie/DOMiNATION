# Rendering

- Supported target resolutions: 1920x1080, 2560x1440, 3840x2160.
- Window resize path handles `SDL_WINDOWEVENT_SIZE_CHANGED` by updating viewport, projection, picking math, and UI anchors.
- CLI controls: `--width`, `--height`, `--fullscreen`, `--borderless`, `--render-scale`, `--ui-scale`.
- Render scaling uses offscreen framebuffer rendering then blits/upscales to display resolution.
- HUD and debug overlays scale with `--ui-scale` for 4K readability.


## Logistics overlays

- Roads render as strategic colored line segments.
- Active trade routes render as yellow strategic lines.
- Low/out-of-supply units are tinted (amber/red).


## Water/coast rendering
- Terrain rendering now colors shallow and deep water separately from land.
- Minimap colors land/shallow/deep water distinctly and still overlays units/cities.

## Visual style guide

Stylized RTS game art should prioritize readability at strategic zoom levels.

### Camera
- Top-down / isometric RTS perspective.

### Art direction
- Clean silhouettes.
- Stylized realism.
- Bright but grounded color palette.
- Clear faction colors.
- High readability from distance.

### Rendering style
- Modern RTS style similar to Rise of Nations, Age of Empires IV, Northgard, and stylized Company of Heroes.

### Lighting
- Soft global illumination.
- Clear shadow shapes.
- Strong contrast between terrain types.

### Scale
- Assets must remain readable when units are small on screen.

### Texture quality
- Clean materials.
- Avoid noisy details.
- Prioritize strong shape readability.

### Resolution targets
- Usable at 1080p / 1440p / 4K.

### Background
- Use a transparent or neutral backdrop for sprite extraction.
