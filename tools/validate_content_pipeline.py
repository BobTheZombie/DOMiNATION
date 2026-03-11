#!/usr/bin/env python3
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "content"
ASSET_ID_RE = re.compile(r"^[a-z0-9]+(?:_[a-z0-9]+)+$")
REQUIRED_TERRAIN_SETS = {
    "temperate_grass",
    "steppe_dry",
    "forest_floor",
    "sand_dune",
    "mediterranean_soil",
    "jungle_floor",
    "tundra_soil",
    "snow",
    "marsh",
    "rock_highland",
    "coastal_mix",
}
REQUIRED_STRUCTURE_CATEGORIES = {
    "city_center",
    "capital",
    "industrial",
    "port",
    "rail",
    "mine",
    "guardian_site",
    "strategic_structure",
}


def load(path: Path):
    with path.open('r', encoding='utf-8') as f:
        return json.load(f)


def validate_required_manifests():
    required = [
        "biome_manifest.json",
        "civilization_theme_manifest.json",
        "asset_manifest.json",
        "atlas_manifest.json",
        "lod_manifest.json",
        "terrain_styles.json",
        "unit_styles.json",
        "building_styles.json",
        "object_styles.json",
    ]
    for name in required:
        path = CONTENT / name
        assert path.exists(), f"missing manifest: {name}"


def validate_assets(asset_manifest, atlas_manifest, lod_manifest):
    warnings = []
    seen = set()
    duplicates = set()
    asset_ids = set()
    for asset in asset_manifest.get("assets", []):
        aid = asset.get("asset_id", "")
        assert aid, "asset missing asset_id"
        if not ASSET_ID_RE.match(aid):
            warnings.append(f"asset naming nonconforming: {aid}")
        if aid in seen:
            duplicates.add(aid)
        seen.add(aid)
        asset_ids.add(aid)

        mesh = asset.get("mesh", "")
        assert isinstance(mesh, str), f"asset {aid} mesh must be a string"
        icon = asset.get("icon", "")
        assert isinstance(icon, str), f"asset {aid} icon must be a string"
        lods = asset.get("lods", [])
        assert isinstance(lods, list), f"asset {aid} lods must be a list"
        if not lods:
            warnings.append(f"asset {aid} has no lod entries")

    assert not duplicates, f"duplicate asset IDs: {sorted(duplicates)}"

    atlas_ids = {a["atlas_id"] for a in atlas_manifest.get("atlases", [])}
    sprite_ids = set()
    for sprite in atlas_manifest.get("sprites", []):
        sid = sprite.get("sprite_id", "")
        assert sid, "sprite missing sprite_id"
        assert sprite.get("atlas") in atlas_ids, f"sprite {sid} references missing atlas {sprite.get('atlas')}"
        sprite_ids.add(sid)

    lod_ids = set()
    lod_by_asset = {}
    for lod in lod_manifest.get("lod_entries", []):
        lid = lod.get("lod_id", "")
        assert lid, "lod entry missing lod_id"
        if not ASSET_ID_RE.match(lid):
            warnings.append(f"lod naming nonconforming: {lid}")
        aid = lod.get("asset_id", "")
        assert aid, f"lod {lid} missing asset_id"
        lod_ids.add(lid)
        lod_by_asset.setdefault(aid, []).append(lod)

    for asset in asset_manifest.get("assets", []):
        aid = asset["asset_id"]
        for lod in asset.get("lods", []):
            assert lod in lod_ids, f"asset {aid} references missing lod id {lod}"
        if aid not in sprite_ids:
            warnings.append(f"asset {aid} has no sprite entry in atlas_manifest")

    for aid, entries in lod_by_asset.items():
        if aid not in asset_ids and aid != "missing_mesh":
            warnings.append(f"lod entries reference unknown asset_id: {aid}")
        screen_sizes = [float(e.get("screen_size", 0.0)) for e in entries]
        if len(entries) >= 2 and sorted(screen_sizes, reverse=True) != screen_sizes:
            warnings.append(f"lod screen_size ordering non-descending for {aid}")

    return warnings


def validate_civ_variants(theme_manifest, asset_manifest):
    warnings = []
    asset_ids = {a["asset_id"] for a in asset_manifest.get("assets", [])}
    required_civ_themes = {'rome', 'china', 'europe', 'middle_east', 'russia', 'usa', 'japan', 'eu', 'uk', 'egypt', 'tartaria'}
    for theme in theme_manifest.get("themes", []):
        mappings = theme.get("building_family_mappings", {})
        required = (
            'House', 'Farm', 'Market', 'Barracks', 'CityCenter', 'Port', 'FactoryHub', 'RailStation', 'Tower'
        ) if theme.get('id') in required_civ_themes else (
            'House', 'Farm', 'Market', 'Barracks', 'CityCenter', 'Port', 'Wonder'
        )
        for fam in required:
            assert fam in mappings, f"theme {theme['id']} missing {fam}"
            mapped = mappings[fam]
            if mapped not in asset_ids:
                warnings.append(f"theme {theme['id']} mapping {fam}->{mapped} missing in asset_manifest")
    return warnings


def validate_biome_coverage(biomes):
    present = {b.get("terrain_material_set", "") for b in biomes.get("biomes", [])}
    missing = sorted(REQUIRED_TERRAIN_SETS - present)
    assert not missing, f"biome_manifest missing terrain material sets: {missing}"


def validate_lod_manifest_metadata(lod_manifest):
    categories = {entry.get("category", "") for entry in lod_manifest.get("lod_entries", []) if entry.get("category")}
    if categories:
        missing = sorted(REQUIRED_STRUCTURE_CATEGORIES - categories)
        assert not missing, f"lod_manifest category coverage missing: {missing}"



def collect_style_refs(style_blob, refs):
    if isinstance(style_blob, dict):
        for key, value in style_blob.items():
            if key == "mesh" and isinstance(value, str) and value:
                refs["mesh"].add(value)
            elif key == "lod_group" and isinstance(value, str) and value:
                refs["lod"].add(value)
            elif key == "icon" and isinstance(value, str) and value:
                refs["icon"].add(value)
            elif key == "state_variants" and isinstance(value, dict):
                for state_key in value.keys():
                    assert state_key in {"default", "construction", "damaged", "selected", "low_supply", "strategic_warning"}, f"invalid style state variant key: {state_key}"
                for sub in value.values():
                    collect_style_refs(sub, refs)
            else:
                collect_style_refs(value, refs)
    elif isinstance(style_blob, list):
        for v in style_blob:
            collect_style_refs(v, refs)


def validate_render_stylesheets(asset_manifest, lod_manifest, atlas_manifest):
    warnings = []
    style_files = [
        CONTENT / "terrain_styles.json",
        CONTENT / "unit_styles.json",
        CONTENT / "building_styles.json",
        CONTENT / "object_styles.json",
    ]
    asset_ids = {a.get("asset_id") for a in asset_manifest.get("assets", [])}
    lod_ids = {l.get("lod_id") for l in lod_manifest.get("lod_entries", [])}
    icon_ids = {s.get("sprite_id") for s in atlas_manifest.get("sprites", [])}
    refs = {"mesh": set(), "lod": set(), "icon": set()}

    for sf in style_files:
      data = load(sf)
      assert data.get("render_classes"), f"{sf.name} missing render_classes"
      assert "default" in data, f"{sf.name} missing default fallback style"
      collect_style_refs(data, refs)
      ids = set()
      for cls, cls_data in data.get("render_classes", {}).items():
        assert re.match(r"^[a-z0-9_]+$", cls), f"invalid render_class name {cls} in {sf.name}"
        sid = cls_data.get("default", {}).get("style_id")
        if sid:
          assert sid not in ids, f"duplicate style_id {sid} in {sf.name}"
          ids.add(sid)

    for mesh in sorted(refs["mesh"]):
      if mesh not in asset_ids and mesh != "missing_mesh":
        warnings.append(f"render stylesheet mesh reference missing in asset_manifest: {mesh}")
    for lod in sorted(refs["lod"]):
      if lod not in lod_ids and lod != "missing_mesh":
        warnings.append(f"render stylesheet lod_group reference missing in lod_manifest: {lod}")
    for icon in sorted(refs["icon"]):
      if icon not in icon_ids and icon != "missing_icon":
        warnings.append(f"render stylesheet icon reference missing in atlas_manifest: {icon}")

    return warnings

def main():
    validate_required_manifests()

    biomes = load(CONTENT / 'biome_manifest.json')
    themes = load(CONTENT / 'civilization_theme_manifest.json')
    assets = load(CONTENT / 'asset_manifest.json')
    atlas = load(CONTENT / 'atlas_manifest.json')
    lod = load(CONTENT / 'lod_manifest.json')

    assert biomes.get('biomes'), 'biome_manifest.json missing biomes'
    assert themes.get('themes'), 'civilization_theme_manifest.json missing themes'
    assert assets.get('assets') is not None, 'asset_manifest.json missing assets'

    warnings = []
    warnings.extend(validate_assets(assets, atlas, lod))
    warnings.extend(validate_civ_variants(themes, assets))
    warnings.extend(validate_render_stylesheets(assets, lod, atlas))
    validate_biome_coverage(biomes)
    validate_lod_manifest_metadata(lod)

    for warning in sorted(set(warnings)):
        print(f"warning: {warning}")

    print('content pipeline schemas validated')


if __name__ == '__main__':
    main()
