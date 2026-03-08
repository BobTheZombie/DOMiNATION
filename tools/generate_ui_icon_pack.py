#!/usr/bin/env python3
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw

OUT_ROOT = Path("content/textures/ui/icon_pack")
SIZES = [16, 24, 32, 48, 64, 128]

PALETTE = {
    "base": (180, 186, 194, 255),
    "line": (228, 233, 240, 255),
    "shadow": (34, 40, 48, 255),
    "danger": (168, 44, 48, 255),
    "economy": (92, 130, 86, 255),
    "info": (92, 140, 170, 255),
    "industry": (120, 116, 102, 255),
    "special": (198, 175, 112, 255),
}

GROUPS: dict[str, list[str]] = {
    "core_resources": ["food", "wood", "metal", "wealth", "knowledge", "oil", "population", "influence", "stability", "world_tension"],
    "industrial_goods": ["steel", "fuel", "munitions", "machine_parts", "electronics", "concrete", "chemicals"],
    "building_families": [
        "house", "farm", "market", "barracks", "city_center", "capital", "port", "dockyard", "factory", "steel_mill", "refinery",
        "munitions_plant", "machine_works", "electronics_lab", "rail_station", "rail_hub", "warehouse", "supply_depot", "radar_station",
        "missile_silo", "fortress", "tower", "wall", "mine", "deep_mine", "tunnel_entrance", "underground_depot", "wonder"
    ],
    "unit_families": [
        "worker", "engineer", "scout", "infantry", "heavy_infantry", "ranged_infantry", "cavalry", "camel_cavalry", "artillery", "siege_unit", "tank", "heavy_tank", "railgun_unit", "mechanized_infantry", "anti_air", "truck", "train", "supply_train", "freight_train", "naval_transport", "destroyer", "cruiser", "battleship", "carrier", "submarine", "recon_drone", "strike_drone", "fighter", "interceptor", "bomber", "strategic_bomber", "missile_unit", "special_forces", "guardian_unit"
    ],
    "civilization_emblems": ["rome", "china", "europe", "middle_east", "russia", "usa", "japan", "eu", "uk", "egypt", "tartaria"],
    "strategic_diplomacy": [
        "war", "alliance", "trade_agreement", "ceasefire", "embargo", "bloc_membership", "rivalry", "neutrality", "influence_expansion", "theater_operation", "offensive", "defensive_line", "encirclement", "blockade", "bombing_campaign", "strategic_warning", "armageddon", "nuclear_launch", "missile_intercept", "second_strike", "launch_on_warning", "no_first_use", "massive_retaliation"
    ],
    "campaign_objectives": ["primary_objective", "secondary_objective", "hidden_objective", "completed_objective", "failed_objective", "briefing", "debrief", "campaign_reward", "carryover_state", "mission_branch", "victory", "defeat", "partial_victory"],
    "world_event_crisis": ["drought", "famine", "plague", "industrial_strike", "market_crash", "rebellion", "fuel_crisis", "severe_winter", "guardian_awakening", "strategic_crisis", "fallout_zone", "global_panic"],
    "mythic_guardians": ["snow_yeti", "kraken", "sandworm", "forest_spirit", "yeti_lair", "abyssal_trench", "dune_nest", "sacred_grove"],
    "command_control": ["move", "attack", "patrol", "hold_position", "build", "repair", "mine", "deploy_rail", "launch_strike", "intercept", "fortify", "retreat", "resupply", "airlift", "naval_invasion", "tunnel_travel", "guardian_claim"],
}

COLOR_HINTS = {
    "danger": {"war", "world_tension", "missile", "armageddon", "nuclear", "retaliation", "crash", "rebellion", "defeat", "bombing", "offensive", "rivalry", "plague", "fallout", "panic", "strike"},
    "economy": {"food", "wood", "farm", "market", "warehouse", "supply", "resupply", "mine", "rail", "trade", "freight", "carryover"},
    "info": {"knowledge", "electronics", "radar", "drone", "fighter", "intercept", "briefing", "debrief", "objective", "warning", "science", "recon", "neutrality"},
    "special": {"capital", "wonder", "victory", "alliance", "influence", "stability", "eu", "uk", "rome", "egypt", "tartaria", "guardian", "sacred"},
    "industry": {"steel", "metal", "factory", "mill", "refinery", "munitions", "machine", "concrete", "chemicals", "tank", "artillery", "silo"},
}


def select_color(name: str) -> tuple[int, int, int, int]:
    n = name.lower()
    for palette_name, keys in COLOR_HINTS.items():
        if any(k in n for k in keys):
            return PALETTE[palette_name]
    return PALETTE["base"]


def draw_chevron(draw: ImageDraw.ImageDraw, c: int, r: int, color):
    draw.polygon([(c-r, c-r//2), (c, c+r//2), (c+r, c-r//2), (c+r//2, c-r//2), (c, c), (c-r//2, c-r//2)], fill=color)

def draw_shield(draw, c, r, color):
    pts = [(c-r, c-r), (c+r, c-r), (c+r, c), (c, c+r), (c-r, c)]
    draw.polygon(pts, fill=color)

def draw_gear(draw, c, r, color):
    for i in range(8):
        a = i * math.pi / 4
        x1, y1 = c + int((r+2)*math.cos(a)), c + int((r+2)*math.sin(a))
        x2, y2 = c + int((r+7)*math.cos(a)), c + int((r+7)*math.sin(a))
        draw.line((x1, y1, x2, y2), fill=color, width=3)
    draw.ellipse((c-r, c-r, c+r, c+r), fill=color)
    draw.ellipse((c-r//3, c-r//3, c+r//3, c+r//3), fill=(0,0,0,0))


def draw_icon_128(name: str) -> Image.Image:
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = 64
    color = select_color(name)
    line = PALETTE["line"]
    shadow = PALETTE["shadow"]

    # Base medal backing for cohesion
    d.ellipse((10, 10, 118, 118), fill=(22, 28, 36, 220), outline=shadow, width=4)
    d.ellipse((18, 18, 110, 110), outline=(66, 72, 80, 200), width=2)

    n = name.lower()
    if any(k in n for k in ["food", "farm", "drought", "famine"]):
        for x in [50, 64, 78]:
            d.line((x, 34, x, 90), fill=color, width=4)
            for y in [44, 56, 68, 80]:
                d.ellipse((x-7, y-4, x, y+4), fill=line)
    elif any(k in n for k in ["wood", "forest", "grove"]):
        d.rectangle((38, 56, 90, 84), fill=color)
        d.ellipse((34, 44, 62, 72), fill=line)
        d.ellipse((66, 44, 94, 72), fill=line)
    elif any(k in n for k in ["oil", "fuel", "chem", "plague"]):
        d.polygon([(64, 30), (86, 66), (64, 98), (42, 66)], fill=color)
        d.ellipse((52, 52, 76, 76), fill=line)
    elif any(k in n for k in ["metal", "steel", "concrete", "machine", "electronics", "factory", "mill", "works", "lab"]):
        draw_gear(d, c, 24, color)
        d.rectangle((52, 52, 76, 76), fill=line)
    elif any(k in n for k in ["wealth", "market", "reward"]):
        d.ellipse((42, 44, 86, 88), fill=color)
        d.ellipse((50, 52, 78, 80), outline=line, width=4)
    elif any(k in n for k in ["knowledge", "brief", "debrief", "objective"]):
        d.rectangle((38, 42, 90, 86), fill=color)
        d.line((64, 42, 64, 86), fill=line, width=3)
        d.line((44, 54, 58, 54), fill=line, width=3)
        d.line((70, 54, 84, 54), fill=line, width=3)
    elif any(k in n for k in ["population", "worker", "infantry", "forces", "guardian"]):
        d.ellipse((50, 36, 78, 64), fill=color)
        d.rectangle((46, 64, 82, 92), fill=color)
        d.rectangle((38, 70, 46, 90), fill=line)
        d.rectangle((82, 70, 90, 90), fill=line)
    elif any(k in n for k in ["influence", "alliance", "bloc", "diplom", "ceasefire", "neutrality"]):
        d.polygon([(64, 30), (90, 52), (64, 96), (38, 52)], fill=color)
        d.line((64, 34, 64, 92), fill=line, width=4)
    elif any(k in n for k in ["stability", "defensive", "fort", "tower", "wall", "hold"]):
        draw_shield(d, c, 30, color)
        d.line((44, 54, 84, 54), fill=line, width=4)
    elif any(k in n for k in ["tension", "war", "armageddon", "crisis", "warning", "panic", "retaliation", "launch"]):
        d.ellipse((36, 36, 92, 92), outline=color, width=8)
        d.line((42, 42, 86, 86), fill=color, width=6)
        d.line((86, 42, 42, 86), fill=line, width=4)
    elif any(k in n for k in ["house", "city", "capital", "barracks", "port", "dock", "depot", "warehouse", "station", "hub", "silo"]):
        d.polygon([(34, 60), (64, 34), (94, 60)], fill=line)
        d.rectangle((40, 60, 88, 92), fill=color)
    elif any(k in n for k in ["mine", "tunnel", "trench", "nest"]):
        d.arc((34, 42, 94, 102), 0, 180, fill=color, width=10)
        d.rectangle((40, 70, 88, 84), fill=line)
    elif any(k in n for k in ["rail", "train"]):
        d.rectangle((32, 50, 96, 82), fill=color)
        d.line((36, 90, 92, 90), fill=line, width=4)
        d.line((44, 82, 52, 98), fill=line, width=3)
        d.line((76, 82, 84, 98), fill=line, width=3)
    elif any(k in n for k in ["radar", "recon", "intercept", "missile"]):
        d.arc((34, 34, 94, 94), 210, 330, fill=color, width=5)
        d.arc((44, 44, 84, 84), 210, 330, fill=line, width=4)
        d.line((64, 64, 84, 44), fill=color, width=4)
    elif any(k in n for k in ["tank", "truck", "artillery", "siege", "cavalry", "camel", "mechanized", "anti_air"]):
        d.rectangle((32, 62, 96, 86), fill=color)
        d.rectangle((54, 50, 78, 64), fill=line)
        d.line((78, 56, 102, 52), fill=line, width=4)
        d.ellipse((38, 84, 54, 100), fill=shadow)
        d.ellipse((74, 84, 90, 100), fill=shadow)
    elif any(k in n for k in ["naval", "destroyer", "cruiser", "battleship", "carrier", "submarine", "blockade", "invasion"]):
        d.polygon([(28, 78), (96, 78), (82, 94), (40, 94)], fill=color)
        d.rectangle((52, 58, 76, 78), fill=line)
        d.arc((28, 88, 100, 106), 200, 340, fill=line, width=3)
    elif any(k in n for k in ["fighter", "bomber", "airlift", "drone", "strike"]):
        d.polygon([(64, 30), (78, 66), (104, 74), (78, 82), (64, 98), (50, 82), (24, 74), (50, 66)], fill=color)
    elif any(k in n for k in ["rome", "china", "europe", "middle_east", "russia", "usa", "japan", "eu", "uk", "egypt", "tartaria"]):
        d.ellipse((30, 30, 98, 98), fill=color)
        draw_chevron(d, 64, 20, line)
    elif any(k in n for k in ["kraken", "sandworm", "yeti", "spirit", "awakening"]):
        d.polygon([(64, 30), (92, 52), (84, 90), (44, 96), (36, 54)], fill=color)
        d.line((50, 60, 78, 54), fill=line, width=4)
        d.line((48, 76, 76, 70), fill=line, width=4)
    elif any(k in n for k in ["move", "attack", "patrol", "build", "repair", "retreat", "deploy", "fortify", "claim"]):
        d.polygon([(34, 64), (78, 36), (78, 52), (96, 52), (96, 76), (78, 76), (78, 92)], fill=color)
    else:
        draw_shield(d, c, 26, color)

    d.ellipse((10, 10, 118, 118), outline=(200, 205, 214, 120), width=1)
    return img


def main() -> None:
    manifest: dict[str, dict[str, list[str]]] = {}
    for group, names in GROUPS.items():
        manifest[group] = {}
        for icon_name in names:
            base = draw_icon_128(icon_name)
            paths: list[str] = []
            for s in SIZES:
                out_dir = OUT_ROOT / group / f"{s}x{s}"
                out_dir.mkdir(parents=True, exist_ok=True)
                out_path = out_dir / f"{icon_name}.png"
                resized = base.resize((s, s), Image.Resampling.LANCZOS)
                resized.save(out_path)
                paths.append(str(out_path))
            manifest[group][icon_name] = paths

    manifest_path = OUT_ROOT / "icon_pack_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
