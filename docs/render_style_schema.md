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


## Validation expectations in Studio
- Render class names should map to supported engine classes (terrain, unit, building, object/site/city_cluster/icon/marker).
- `lod_group`/LOD references should resolve to `lod_manifest.json` entries.
- Attachment mapping keys should use supported hook names (`banner_socket`, `civ_emblem`, `smoke_stack`, `muzzle_flash`, `selection_badge`, `warning_badge`, `guardian_aura`).


## Scene context validation notes
Studio scene placements keep resolved style IDs and mesh refs as editor-only preview state. Missing or invalid refs are surfaced as warnings in Studio logs/validation UI and continue to fail safely without mutating runtime simulation authority.


Studio scene layout persistence (`tools/dom_asset_studio/scene_preview_layout.json`) is editor-only preview state and is intentionally outside render style schema/runtime data.
