#!/usr/bin/env python3
import json
from pathlib import Path


def load(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def main():
    biomes = load('content/biomes.json')
    themes = load('content/civilization_themes.json')
    manifest = load('content/asset_manifest.json')

    assert biomes.get('biomes'), 'biomes.json missing biomes'
    assert themes.get('themes'), 'civilization_themes.json missing themes'
    assert manifest.get('assets') is not None, 'asset_manifest.json missing assets'

    required = {'rome', 'china', 'europe', 'middleeast'}
    got = {t['id'] for t in themes['themes']}
    missing = required - got
    assert not missing, f'missing required themes: {sorted(missing)}'

    for t in themes['themes']:
        m = t.get('building_family_mappings', {})
        for fam in ('House', 'Farm', 'Market', 'Barracks', 'CityCenter', 'Port', 'Wonder'):
            assert fam in m, f"theme {t['id']} missing {fam}"

    print('content pipeline schemas validated')


if __name__ == '__main__':
    main()
