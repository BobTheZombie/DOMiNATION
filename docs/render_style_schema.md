# Render Style JSON Schema (practical)

Top-level keys:
- `default`
- `exact_mappings`
- `render_classes`

Style entry keys:
- `style_id`, `mesh`, `material`, `material_set`, `lod_group`, `icon`, `badge`, `decal_set`
- `tint: [r,g,b]`
- `size_scale: [x,y]`
- `attachments: { socket_or_hook: attachment_style_id }`
- `state_variants` (`construction`, `damaged`, `selected`, `low_supply`, `strategic_warning`)
- `lods` (`near`, `mid`, `far`)

## DOM Asset Studio structured editing
The Studio exposes structured form fields for core schema keys (`render_classes.default`, `civ_overrides`, `theme_overrides`, `lod_group`, `attachments`) with optional raw JSON inspection.
