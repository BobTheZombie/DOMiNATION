#!/usr/bin/env python3
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "content"


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
    ]
    for name in required:
        path = CONTENT / name
        assert path.exists(), f"missing manifest: {name}"


def validate_assets(asset_manifest, atlas_manifest, lod_manifest):
    seen = set()
    duplicates = set()
    for asset in asset_manifest.get("assets", []):
        aid = asset.get("asset_id", "")
        assert aid, "asset missing asset_id"
        if aid in seen:
            duplicates.add(aid)
        seen.add(aid)

        mesh = asset.get("mesh", "")
        assert isinstance(mesh, str), f"asset {aid} mesh must be a string"
        icon = asset.get("icon", "")
        assert isinstance(icon, str), f"asset {aid} icon must be a string"

    assert not duplicates, f"duplicate asset IDs: {sorted(duplicates)}"

    atlas_ids = {a["atlas_id"] for a in atlas_manifest.get("atlases", [])}
    sprite_ids = set()
    for sprite in atlas_manifest.get("sprites", []):
        sid = sprite.get("sprite_id", "")
        assert sid, "sprite missing sprite_id"
        assert sprite.get("atlas") in atlas_ids, f"sprite {sid} references missing atlas {sprite.get('atlas')}"
        sprite_ids.add(sid)

    lod_ids = set()
    for lod in lod_manifest.get("lod_entries", []):
        lid = lod.get("lod_id", "")
        assert lid, "lod entry missing lod_id"
        lod_ids.add(lid)

    for asset in asset_manifest.get("assets", []):
        aid = asset["asset_id"]
        for lod in asset.get("lods", []):
            assert lod in lod_ids, f"asset {aid} references missing lod id {lod}"
        if aid not in sprite_ids:
            print(f"warning: asset {aid} has no sprite entry in atlas_manifest")


def validate_civ_variants(theme_manifest, asset_manifest):
    asset_ids = {a["asset_id"] for a in asset_manifest.get("assets", [])}
    required_civ_themes = {'rome', 'china', 'europe', 'middle_east', 'russia', 'usa', 'japan', 'eu', 'uk', 'egypt', 'tartaria'}
    for theme in theme_manifest.get("themes", []):
        mappings = theme.get("building_family_mappings", {})
        required = ('House', 'Farm', 'Market', 'Barracks', 'CityCenter', 'Port', 'FactoryHub', 'RailStation', 'Tower') if theme.get('id') in required_civ_themes else ('House', 'Farm', 'Market', 'Barracks', 'CityCenter', 'Port', 'Wonder')
        for fam in required:
            assert fam in mappings, f"theme {theme['id']} missing {fam}"
            mapped = mappings[fam]
            if mapped not in asset_ids:
                print(f"warning: theme {theme['id']} mapping {fam}->{mapped} missing in asset_manifest")


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

    validate_assets(assets, atlas, lod)
    validate_civ_variants(themes, assets)

    print('content pipeline schemas validated')


if __name__ == '__main__':
    main()
