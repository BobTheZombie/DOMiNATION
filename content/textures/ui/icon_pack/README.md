# DOMiNATION RTS UI Icon Pack

This pack contains transparent PNG command-panel icons generated for a cohesive grand-strategy RTS UI style.

## Structure

- `content/textures/ui/icon_pack/<group>/<size>x<size>/<icon_name>.png`
- Supported sizes: `16, 24, 32, 48, 64, 128`
- Groups:
  - `core_resources`
  - `industrial_goods`
  - `building_families`
  - `unit_families`
  - `civilization_emblems`
  - `strategic_diplomacy`
  - `campaign_objectives`
  - `world_event_crisis`
  - `mythic_guardians`
  - `command_control`

## Naming Convention

- Snake case icon names (example: `world_tension.png`, `launch_on_warning.png`)
- One file per icon per size
- Transparent backgrounds for atlas packing

## Manifest

`icon_pack_manifest.json` indexes all generated outputs by group and icon name.

## Regeneration

Run:

```bash
python tools/generate_ui_icon_pack.py
```
