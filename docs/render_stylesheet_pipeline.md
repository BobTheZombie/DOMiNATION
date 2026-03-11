# Render Stylesheet Pipeline

Rendering presentation now resolves through JSON stylesheets:

1. exact content mapping
2. civilization override
3. civilization theme override
4. render class default
5. domain default fallback

Files:
- `content/terrain_styles.json`
- `content/unit_styles.json`
- `content/building_styles.json`
- `content/object_styles.json`

Runtime code: `engine/render/render_stylesheet.*`.

The resolver is deterministic and does not mutate simulation state.
