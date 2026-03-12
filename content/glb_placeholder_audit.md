# GLB Placeholder Repair Audit

## Scan scope
- content/asset_manifest.json
- content/lod_manifest.json

## Missing references found before repair
- Total references scanned: 841
- Unique .glb paths scanned: 561
- Missing .glb paths before repair: 561 (all unique paths in scope were missing).

## Repair strategy
- No semantically matching existing .glb assets were available in-repo for reuse.
- Added canonical fallback placeholders under assets_final/fallback/.
- Materialized missing export/meshes/*.glb paths as deterministic placeholder hardlinks/copies by category (unit/building/object).
- Left manifest/style resolution chains unchanged; only missing asset files were added.

## Placeholder files created
- assets_final/fallback/missing_mesh.glb
- assets_final/fallback/placeholder_building.glb
- assets_final/fallback/placeholder_city_cluster.glb
- assets_final/fallback/placeholder_guardian_site.glb
- assets_final/fallback/placeholder_object.glb
- assets_final/fallback/placeholder_unit.glb

## Materialized mesh files
- Total export/meshes placeholder files present: 560
- Representative examples:
  - export/meshes/Archer.glb
  - export/meshes/Archer_lod1.glb
  - export/meshes/Barracks.glb
  - export/meshes/Barracks_lod1.glb
  - export/meshes/BombardShip.glb
  - export/meshes/BombardShip_lod1.glb
  - export/meshes/Bomber.glb
  - export/meshes/Bomber_lod1.glb
  - export/meshes/Cavalry.glb
  - export/meshes/Cavalry_lod1.glb
  - export/meshes/CityCenter.glb
  - export/meshes/CityCenter_lod1.glb
  - ...
  - export/meshes/usa_strategic_bomber_wing_lod1.glb
  - export/meshes/usa_tower_a.glb
  - export/meshes/usa_tower_a_lod1.glb
  - export/meshes/usa_wonder_a.glb
  - export/meshes/usa_wonder_a_lod1.glb

## Post-repair check
- Missing .glb references in scan scope after repair: 0
