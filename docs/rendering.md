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


## Visual feedback pass

Renderer now includes a deterministic feedback pass with category-based overlays for combat, strategic strikes, crises, guardians, industry/logistics activity, and selection/command acknowledgement.

Implementation notes:
- no RNG; phase variation is derived from `world.tick` + stable IDs
- stable iteration order comes from authoritative container order
- Armageddon emits a unique world pulse tint + alert-level emphasis
- denial zones and strategic strike warnings get strategic-scale rings/trails
- rail/factory state emits throughput/blocked pulses

## Territory rendering system (presentation-only)

Territory rendering is a non-authoritative presentation pass built from simulation-owned territory and diplomacy data.

### Borders
- Borders are extracted from ownership discontinuities (`territoryOwner` neighbor tests).
- Colored border accents are derived per owning civ team and relation-aware tinting.
- Border line width/strength scales by zoom to preserve readability at strategic distances and reduce clutter at tactical zoom.

### Ownership readability
- Region tinting is subtle and relation-aware (friendly/allied/neutral/hostile).
- A low-alpha edge glow pass reinforces region separation without changing terrain topology.

### Strategic overlays
Optional strategic overlays are rendered for:
- contested territory seams,
- frontline pressure bands,
- crisis pressure (active world events),
- Armageddon/fallout pressure zones.

### Labels and hooks
Renderer now exposes lightweight label hooks:
- major capitals,
- theaters,
- strategic sites.

Hooks are emitted as deterministic derived metadata for UI/HUD consumers.

### Minimap coherence
Minimap generation now includes:
- ownership tint coherence,
- territory border highlighting,
- capital emphasis,
- theater/crisis pressure markers.

### Determinism and authority boundary
All territory/border/minimap overlays are derived at render time and excluded from simulation authority/save/hash pathways.


## City / region presentation pass

`engine/render/renderer.*` now renders a deterministic city/region presentation layer derived from existing authoritative world state only.

Rules implemented:
- Settlement tiers: small settlement, developed city, large city cluster silhouettes.
- Capital landmarks: capitals receive stronger landmark geometry + ring treatment; level-4+ major centers receive lighter treatment.
- Region markers from existing state:
  - industrial hubs via factory-chain buildings
  - ports via `BuildingType::Port`
  - rail hubs via `RailNodeType::Station/Depot`
  - mining regions via `BuildingType::Mine`
- Civilization-themed settlement silhouette variants: Rome, China, Europe, Middle East, Russia, USA, Japan, EU, UK, Egypt, Tartaria.
- Deterministic fallback order: civ-specific shape -> theme shape -> generic shape -> default fallback counter increment.
- Zoom behavior: near/mid keeps silhouette detail; far zoom scales down to preserve readability.
- Minimap coherence: larger capital dots + deterministic color accents for major region categories.

No city/region render caches are serialized. Presentation reconstructs each frame from authoritative state.


## Strategic movement and logistics visualization layer

A new presentation-only strategic visualization pass renders deterministic overlays reconstructed from authoritative world state.

Features:
- movement intent paths for unit orders (infantry/armor/naval/air/rail-styled variants)
- city/factory/port-to-force supply and logistics flow hints
- rail throughput pulses, train markers, and hub glow markers
- frontline pressure and contested-zone heat overlays
- theater objective direction arrows and objective rings

Determinism rules:
- no authoritative simulation writes are performed by renderer/debug code
- animation phase derives only from `world.tick` + stable IDs
- identical seed/scenario/commands/ticks/threads preserve authoritative hash output

Debug counters (Debug Visualization → Strategic Visualization):
- `MOVEMENT_PATH_RESOLVES`
- `SUPPLY_FLOW_RESOLVES`
- `RAIL_VISUAL_EVENTS`
- `FRONTLINE_ZONE_UPDATES`
- `THEATER_VISUAL_RESOLVES`
- `VISUAL_FALLBACK_COUNT`
- `RAIL_FLOW_LINES`
- `TRAIN_MARKERS`
- `LOGISTICS_VISUAL_EVENTS`


## Deterministic content resolution

The renderer now shares a single ordered content fallback model (exact -> civ -> theme -> category -> default) with debug counters for material/entity/city-region/icon resolution and fallback totals.
## Production world rendering pass (terrain/unit/object/civ/LOD)

- Terrain material pass now emphasizes biome readability for grassland, steppe, forest ground, desert, mediterranean, jungle, tundra, snow/arctic, wetlands, mountain/snow mountain, coast/littoral, shallow/deep water, and rivers/lakes.
- Additional deterministic coast and water-adjacency blending improves shoreline and river/lake legibility while keeping minimap coherence.
- Unit rendering uses deterministic category glyph families (worker, infantry, ranged/heavy infantry, cavalry/raider, artillery/siege, armor/mech, rail/train, naval, aircraft, guardian) with civ theme tinting and zoom-based simplification.
- Structure rendering differentiates civic/military/industrial/logistics/strategic/mythic categories via deterministic shape and accent treatment.
- Civ identity handling includes Rome, China, Europe, Middle East, Russia, USA, Japan, EU, UK, Egypt, and Tartaria-specific tint and settlement silhouette routing with stable fallback.
- LOD behavior: near keeps richer silhouettes, mid simplifies, far clusters armies while preserving strategic readability.
