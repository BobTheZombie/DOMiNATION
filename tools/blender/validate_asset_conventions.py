#!/usr/bin/env python3
import json
import re
from pathlib import Path

NAME_RE = re.compile(r"^(?:[a-z0-9]+(?:_[a-z0-9]+)+|[A-Za-z][A-Za-z0-9]*)$")
LOD_RE = re.compile(r"^(?:[a-z0-9]+(?:_[a-z0-9]+)+|[A-Za-z][A-Za-z0-9]*)_lod[0-9]+$")
RECOMMENDED_FAMILIES = ["House", "Farm", "Granary", "LumberCamp", "Mine", "Market", "Barracks", "Stable", "SiegeWorkshop", "CityCenter", "Port", "Wonder", "Wall", "Tower"]
ALLOWED_TYPES = {"building", "unit", "terrain", "resource", "structure", "icon", "marker", "portrait", "fx", "ui"}


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def validate_manifest(path: Path, warnings):
    data = load_json(path)
    for asset in data.get("assets", []):
        aid = asset.get("asset_id", "")
        if not NAME_RE.match(aid):
            warnings.append(f"invalid asset_id naming: {aid}")

        typ = asset.get("type", "")
        if typ and typ not in ALLOWED_TYPES:
            warnings.append(f"asset has uncommon type '{typ}': {aid}")

        lods = asset.get("lods", [])
        if lods and not isinstance(lods, list):
            warnings.append(f"asset lods should be list: {aid}")
            lods = []
        for lod in lods:
            if not LOD_RE.match(lod):
                warnings.append(f"asset {aid} has unusual lod id format: {lod}")

        if asset.get("type") == "building" and not asset.get("construction_state_asset"):
            warnings.append(f"building missing construction-state linkage: {aid}")


def validate_themes(path: Path, warnings):
    data = load_json(path)
    for theme in data.get("themes", []):
        mappings = theme.get("building_family_mappings", {})
        for fam in RECOMMENDED_FAMILIES:
            if fam not in mappings:
                warnings.append(f"theme {theme.get('id')} missing recommended family mapping: {fam}")


def main():
    warnings = []
    validate_manifest(Path("content/asset_manifest.json"), warnings)
    validate_themes(Path("content/civilization_themes.json"), warnings)

    for warning in sorted(set(warnings)):
        print(f"warning: {warning}")

    print("Asset conventions validated")


if __name__ == "__main__":
    main()
