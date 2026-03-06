#!/usr/bin/env python3
import json
import re
from pathlib import Path

NAME_RE = re.compile(r"^(?:civ_[a-z0-9]+_[a-z0-9]+(?:_[a-z0-9]+)?|biome_[a-z0-9]+_[a-z0-9]+(?:_[a-z0-9]+)?|neutral_[a-z0-9]+_[a-z0-9]+(?:_[a-z0-9]+)?|[a-z0-9]+_[a-z0-9]+_[a-z0-9]+)$")
REQUIRED_FAMILIES = ["House", "Farm", "Granary", "LumberCamp", "Mine", "Market", "Barracks", "Stable", "SiegeWorkshop", "CityCenter", "Port", "Wonder", "Wall", "Tower"]


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def validate_manifest(path: Path, errors):
    data = load_json(path)
    for asset in data.get("assets", []):
        aid = asset.get("asset_id", "")
        if not NAME_RE.match(aid):
            errors.append(f"invalid asset_id naming: {aid}")
        if asset.get("type") == "building" and not asset.get("construction_state_asset"):
            errors.append(f"building missing construction-state linkage: {aid}")


def validate_themes(path: Path, errors):
    data = load_json(path)
    for theme in data.get("themes", []):
        mappings = theme.get("building_family_mappings", {})
        for fam in REQUIRED_FAMILIES:
            if fam not in mappings:
                errors.append(f"theme {theme.get('id')} missing family mapping: {fam}")


def main():
    errors = []
    validate_manifest(Path("content/asset_manifest.json"), errors)
    validate_themes(Path("content/civilization_themes.json"), errors)
    if errors:
        for err in errors:
            print(f"ERROR: {err}")
        raise SystemExit(1)
    print("Asset conventions validated")


if __name__ == "__main__":
    main()
