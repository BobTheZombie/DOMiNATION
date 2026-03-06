# Unit Sprites

## `worker_unit_rts_sprite_256.png`
- Resolution: 256x256.
- Background: transparent (RGBA) for placement over terrain tiles.
- Perspective: top-down RTS sprite.
- Style target: civilian worker carrying construction tools.
- Gameplay target: clear worker silhouette and readable team-color sash at strategic zoom.

Generated via `python3 tools/generate_worker_unit.py`.

## `infantry_unit_rts_sprite_256.png`
- Resolution: 256x256.
- Background: transparent (RGBA) for placement over terrain tiles.
- Perspective: top-down RTS sprite.
- Equipment: armor, sword, and shield.
- Style target: compact heavy infantry silhouette with broad shield and angled sword.
- Gameplay target: remains recognizable at small size with strong contour and a visible team-color accent.

Generated via `python3 tools/generate_infantry_unit.py`.
