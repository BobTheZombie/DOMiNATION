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

VALID_STYLE_STATES = {"default", "construction", "damaged", "selected", "low_supply", "strategic_warning"}


def validate_render_styles(path: Path, warnings):
    data = load_json(path)
    if "default" not in data:
        warnings.append(f"{path.name} missing default fallback")
    for cls in data.get("render_classes", {}).keys():
        if not re.match(r"^[a-z0-9_]+$", cls):
            warnings.append(f"{path.name} invalid render class id: {cls}")
    def walk(node):
        if isinstance(node, dict):
            for k, v in node.items():
                if k == "state_variants" and isinstance(v, dict):
                    for state in v.keys():
                        if state not in VALID_STYLE_STATES:
                            warnings.append(f"{path.name} invalid style state: {state}")
                walk(v)
        elif isinstance(node, list):
            for item in node:
                walk(item)
    walk(data)


def main():
    warnings = []
    validate_manifest(Path("content/asset_manifest.json"), warnings)
    validate_themes(Path("content/civilization_themes.json"), warnings)
    validate_render_styles(Path("content/terrain_styles.json"), warnings)
    validate_render_styles(Path("content/unit_styles.json"), warnings)
    validate_render_styles(Path("content/building_styles.json"), warnings)
    validate_render_styles(Path("content/object_styles.json"), warnings)

    for warning in sorted(set(warnings)):
        print(f"warning: {warning}")

    print("Asset conventions validated")


if __name__ == "__main__":
    main()
