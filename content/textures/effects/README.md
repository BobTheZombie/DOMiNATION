# Effect Sprites

## `explosion_fire_smoke_shockwave_rts_sprite_256.png`
- Resolution: 256x256.
- Background: transparent (RGBA) for compositing over terrain and units.
- Perspective: top-down RTS sprite effect.
- Style target: stylized explosion with a bright fire burst core, surrounding smoke cloud, and an expanding shockwave ring.
- Gameplay target: reads clearly at strategic zoom and packs cleanly into sprite sheets.

Generated via `python3 tools/generate_explosion_effect.py`.

## `missile_trail_flame_smoke_arc_rts_sprite_256.png`
- Resolution: 256x256.
- Background: transparent (RGBA) for compositing over terrain/units.
- Perspective: top-down RTS projectile effect.
- Style target: bright missile flame core with a soft smoke trail.
- Gameplay target: clean curved arc that remains readable at strategic zoom.

Generated via `python3 tools/generate_missile_trail_effect.py`.
