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

## Editor preview overlays
- The renderer supports a lightweight editor preview overlay (tinted circle/ghost marker) for brush/object placement feedback.
- Preview color encodes validity (green valid / red invalid) and remains camera-zoom independent.

## Strategic warfare visuals
- Air units render with unit markers and mission-target indicators.
- Debug/GOD overlays include radar contact coverage and denial-zone circles.
- Strategic warning notifications highlight incoming strike travel phases and interception outcomes.

## Mountain rendering updates
- Added snow-capped mountain biome palette (`snow_capped_mountains`) for strategic visibility.
- Minimap now reflects snow mountain biome color separation.


## Rail rendering hooks

Renderer draws:
- rail segments distinct from roads
- train markers (supply/freight/armored colors) on nodes/edges
- debug panel counters for rail nodes/edges/networks/trains/throughput/disruption

This is intentionally minimal and deterministic-authoritative state driven.

## Visual polish notes
Terrain/minimap now uses stronger biome palettes plus river/lake and elevation cues for readability. Industrial/rail/strategic layers add lightweight overlays for rails, hubs, detector sites, denial zones, and strike warnings.

## Civilization UI/icon fallback behavior

UI content references are manifest-driven. Missing icon/portrait assets resolve to deterministic defaults:
- icon fallback: `ui_icon_*_fallback` / generic event icon classes
- portrait fallback: `ui_portrait_default`

Asset lookup failures remain non-fatal and presentation-only.

## HUD/panel presentation rules
- HUD panel readability targets: 1080p, 1440p, 4K with existing UI/global font scaling.
- Minimap panel now has a dedicated framed region and strategic marker summary text for situational awareness.
- Strategic warnings use explicit semantic colors (warning/success/failure/info) via shared ImGui theme helpers.
- Missing icon/portrait references continue to fall back deterministically to existing placeholders.

## Deterministic terrain presentation pass

The renderer now uses a dedicated terrain material resolver (`engine/render/terrain_materials.*`) to map each cell to a stable material id and palette.

Material classes include:
- grassland, steppe/plains, forest ground, desert, mediterranean, jungle, tundra, snow/arctic, wetlands
- mountain, snow mountain
- littoral coast
- river, lake, shallow ocean, deep ocean

Mountain/cliff readability:
- slope-derived cliff strokes are rendered from heightmap deltas
- snow-cap accents are applied on snow-mountain and high-elevation peaks

Water readability:
- rivers and lakes are resolved before generic water classes
- littoral coastline is derived deterministically from land cells adjacent to water

Feature clutter:
- deterministic forest canopy clusters from biome/material and tile coordinates
- deterministic markers for resource nodes, deep deposits, and revealed guardian sites
- far-zoom marker simplification for strategic readability

Debug integration:
- terrain material overlay toggle
- water overlay toggle
- counters: TERRAIN_MATERIAL_RESOLVES, WATER_FEATURE_RESOLVES, FOREST_CLUSTER_COUNT, MOUNTAIN_FEATURE_COUNT, PRESENTATION_FALLBACK_COUNT


## World entities (units/cities/structures/sites)

The first entity pass adds deterministic shape-based world entities with civ/theme tinting and ownership accents across near/mid/far zoom. Far zoom uses cluster bins for armies, while capitals, industrial cities, strategic structures, and revealed guardian sites keep readable silhouettes.

Debug counters exposed in the visualization panel:
- `UNIT_PRESENTATION_RESOLVES`
- `BUILDING_PRESENTATION_RESOLVES`
- `CITY_PRESENTATION_RESOLVES`
- `GUARDIAN_PRESENTATION_RESOLVES`
- `ENTITY_PRESENTATION_FALLBACKS`
- `FAR_LOD_CLUSTER_COUNT`

## World marker / badge pass
- Strategic world markers for capitals, rail/port/factory/radar/missile entities are rendered as deterministic overlays.
- Selection/low-supply/warning badges are driven from unit/building authoritative state.
- Marker lookups use stable IDs and fallback IDs without mutating simulation state.
