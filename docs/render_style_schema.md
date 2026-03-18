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
- `attachment_anchors: { socket_or_hook: { pos: [x,y,z], radius: number } }` (editor/pipeline authored metadata for preview + renderer hook alignment)
- `state_variants` (`construction`, `damaged`, `selected`, `low_supply`, `strategic_warning`)
- `lods` (`near`, `mid`, `far`)

## DOM Asset Studio structured editing
The Studio exposes structured form fields for core schema keys (`render_classes.default`, `civ_overrides`, `theme_overrides`, `lod_group`, `attachments`, `attachment_anchors`) with optional raw JSON inspection.


## Validation expectations in Studio
- Render class names should map to supported engine classes (terrain, unit, building, object/site/city_cluster/icon/marker).
- `lod_group`/LOD references should resolve to `lod_manifest.json` entries.
- Attachment mapping keys should use supported hook names (`banner_socket`, `civ_emblem`, `smoke_stack`, `muzzle_flash`, `selection_badge`, `warning_badge`, `guardian_aura`).


## Scene context validation notes
Studio scene placements keep resolved style IDs and mesh refs as editor-only preview state. Missing or invalid refs are surfaced as warnings in Studio logs/validation UI and continue to fail safely without mutating runtime simulation authority.


Studio scene layout persistence (`tools/dom_asset_studio/scene_preview_layout.json`) is editor-only preview state and is intentionally outside render style schema/runtime data.

## Runtime model fields
`mesh` and `lod_group` are now consumed by the in-game runtime model pass for deterministic GLB selection. State variants continue to override these fields declaratively.

## `animation` schema fragment
```json
"animation": {
  "default_state": "idle",
  "default_clip": "idle",
  "state_clips": { "move": "move", "attack": "attack" },
  "playback_hints": { "attack": "oneshot", "idle": "loop" }
}
```
All fields are optional; unresolved clips are handled by deterministic fallback.

## Readability fields used by runtime shaders
The declarative `readability` block now supports these bounded fields in addition to the existing lighting/readability values:
- `terrain_blend`: terrain-to-accent blend amount.
- `terrain_macro_variation`: deterministic large-scale terrain breakup strength.
- `terrain_slope_strength`: slope/landform emphasis used by the terrain shader.
- `water_emphasis`: coast/river/water readability emphasis.

These remain declarative JSON values resolved by code and uploaded as shader inputs; they do not execute user-authored logic.
