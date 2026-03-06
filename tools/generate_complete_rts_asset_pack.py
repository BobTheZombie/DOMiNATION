#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import hashlib
import math
import random
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path('content/textures/complete_rts_pack')

TERRAINS = [
    'grassland_terrain_tile','dry_steppe_terrain_tile','dirt_terrain_tile','sand_terrain_tile','snow_terrain_tile','rock_terrain_tile',
    'farmland_soil_tile','crop_field_tile','road_texture','stone_road_texture','bridge_texture','river_water_tile','shallow_water_tile',
    'deep_ocean_tile','coastline_edge_texture','marsh_swamp_terrain_tile'
]
ENVIRONMENT = ['forest_tree_cluster','jungle_tree_cluster','palm_tree_cluster','dead_forest_cluster','rock_outcrop','cliff_edge','large_boulder']
RESOURCES = ['tree_resource','stone_deposit','metal_ore_deposit','gold_deposit','coal_deposit','oil_well','fish_school','livestock_herd','fruit_tree_grove','wheat_field','rice_field','rare_mineral_deposit']
BUILDINGS_ECON = ['house','large_house','farm_structure','barn','granary','lumber_camp','mine','warehouse','market','trade_depot','caravan_station','bank','stock_exchange','library','university','laboratory','capital_city_center','wonder_monument']
BUILDINGS_MIL = ['barracks','archery_range','stable','siege_workshop','armory','fortress','watch_tower','stone_wall','city_gate','military_academy','radar_station','missile_silo','airbase','drone_command_center']
NAVAL_BUILDINGS = ['port','harbor','shipyard','naval_supply_depot']
EARLY = ['worker','scout','spearman','swordsman','archer','horseman','chariot','catapult','ballista','merchant_caravan','early_transport_ship','galley_warship']
MEDIEVAL = ['knight','crossbowman','pikeman','trebuchet','war_elephant','monk_scholar','merchant_ship','war_galley','siege_tower','supply_wagon']
INDUSTRIAL = ['rifleman','cavalry_gunpowder_era','field_artillery','gatling_gun_crew','ironclad_ship','steam_warship','transport_ship','engineer','recon_balloon','early_bomber','machine_gun_crew','armored_car']
MODERN = ['infantry_squad','sniper','heavy_machine_gun_team','tank','heavy_tank','mobile_artillery','rocket_artillery','sam_launcher','attack_helicopter','fighter_jet','bomber_aircraft','transport_helicopter','missile_launcher','naval_destroyer']
INFO = ['recon_drone','strike_drone','advanced_battle_tank','stealth_bomber','missile_submarine','aircraft_carrier','cruise_missile_launcher','anti_missile_defense_system','special_forces_unit','autonomous_ground_vehicle','satellite_uplink_station']
STRATEGIC = ['strategic_missile_silo','nuclear_missile_launcher','tactical_missile_battery','emp_weapon_platform','orbital_strike_marker','radiation_denial_zone_visual']
NAVAL_UNITS = ['fishing_boat','transport_ship','light_warship','heavy_warship','bombard_ship','destroyer','cruiser','aircraft_carrier','submarine','missile_submarine']
VFX = ['explosion_effect','smoke_plume','missile_trail','bullet_tracer','artillery_arc','fire_effect','building_construction_animation','building_collapse_debris','water_splash','drone_rotor_effect','large_detonation_effect','emp_pulse_effect']
UI_RESOURCE = ['food','wood','metal','wealth','knowledge','oil']
UI_UNIT = ['infantry','cavalry','archer','siege','tank','drone','naval_ship','missile']
UI_GAMEPLAY = ['diplomacy','trade_route','supply','alert','world_tension','objective_marker']

PALETTE = {
    'team': (197, 62, 74, 255),
    'team_light': (239, 148, 157, 255),
    'outline': (21, 20, 24, 210),
    'shadow': (0, 0, 0, 110),
}


def stable_rng(name: str) -> random.Random:
    h = hashlib.sha256(name.encode()).hexdigest()
    return random.Random(int(h[:16], 16))


def ensure(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def terrain_colors(name: str):
    if 'grass' in name:
        return (86, 127, 76), (122, 167, 97)
    if 'steppe' in name:
        return (156, 136, 95), (186, 166, 115)
    if 'dirt' in name:
        return (118, 89, 62), (150, 114, 84)
    if 'sand' in name:
        return (198, 175, 121), (223, 202, 149)
    if 'snow' in name:
        return (186, 198, 212), (229, 236, 245)
    if 'rock' in name:
        return (106, 108, 112), (137, 141, 147)
    if 'farmland' in name:
        return (112, 76, 50), (142, 100, 65)
    if 'crop' in name or 'wheat' in name or 'rice' in name:
        return (124, 112, 52), (174, 162, 78)
    if 'road' in name:
        return (102, 90, 75), (142, 126, 104)
    if 'bridge' in name:
        return (122, 92, 62), (158, 124, 88)
    if 'river' in name or 'shallow' in name:
        return (58, 121, 155), (94, 164, 196)
    if 'ocean' in name:
        return (38, 70, 118), (62, 108, 156)
    if 'coastline' in name:
        return (118, 145, 152), (172, 194, 188)
    if 'marsh' in name:
        return (82, 102, 79), (116, 137, 94)
    return (110, 110, 110), (140, 140, 140)


def draw_terrain(name: str, size: int = 1024) -> Image.Image:
    rng = stable_rng(name)
    c0, c1 = terrain_colors(name)
    img = Image.new('RGBA', (size, size), (*c0, 255))
    d = ImageDraw.Draw(img, 'RGBA')

    for y in range(size):
        t = y / max(1, size - 1)
        col = tuple(int(c0[i] * (1 - t) + c1[i] * t) for i in range(3))
        d.line([(0, y), (size, y)], fill=(*col, 255))

    for _ in range(180):
        x = rng.randint(0, size)
        y = rng.randint(0, size)
        r = rng.randint(12, 58)
        a = rng.randint(18, 42)
        tint = rng.randint(-18, 18)
        col = tuple(max(0, min(255, c0[i] + tint)) for i in range(3))
        d.ellipse((x-r, y-r, x+r, y+r), fill=(*col, a))

    if 'road' in name:
        for k in range(-2, 3):
            off = k * 24
            d.line([(0, size*0.5+off), (size, size*0.5+off)], fill=(86, 80, 72, 110), width=8)
    if 'bridge' in name:
        d.rectangle((size*0.42, 0, size*0.58, size), fill=(92, 116, 145, 140))
        for i in range(10):
            x = size * 0.3 + i * size * 0.045
            d.rectangle((x, size*0.1, x+18, size*0.9), fill=(145, 110, 78, 140))
    if 'coastline' in name:
        d.polygon([(0, size*0.6), (size*0.35, size*0.45), (size, size*0.35), (size, size), (0, size)], fill=(186, 170, 132, 235))
    if 'water' in name or 'ocean' in name or 'river' in name:
        for i in range(0, size, 40):
            d.arc((i-80, i*0.6, i+240, i*0.6+110), 10, 170, fill=(220, 236, 248, 48), width=3)

    return img


def draw_iso_sprite(name: str, size: int = 512, cls: str = 'generic') -> Image.Image:
    rng = stable_rng(name + cls)
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, 'RGBA')
    cx, cy = size * 0.5, size * 0.58

    sh = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh, 'RGBA')
    sd.ellipse((cx-140, cy+70, cx+140, cy+148), fill=PALETTE['shadow'])
    sh = sh.filter(ImageFilter.GaussianBlur(8))
    img.alpha_composite(sh)

    d.polygon([(cx, cy-80), (cx+120, cy-12), (cx, cy+56), (cx-120, cy-12)], fill=(108, 118, 128, 255))
    d.polygon([(cx-120, cy-12), (cx, cy+56), (cx, cy+110), (cx-120, cy+44)], fill=(86, 95, 105, 255))
    d.polygon([(cx+120, cy-12), (cx, cy+56), (cx, cy+110), (cx+120, cy+44)], fill=(72, 80, 90, 255))

    # Silhouette body
    height = rng.randint(95, 170)
    width = rng.randint(40, 86)
    top = cy - height
    left = cx - width
    right = cx + width
    body_col = (142, 146, 152, 255)
    if cls in {'unit', 'resource'}:
        body_col = (126, 135, 145, 255)
    if cls == 'building':
        body_col = (149, 137, 112, 255)
    if cls == 'effect':
        body_col = (189, 116, 82, 220)
    d.polygon([(cx, top), (right, cy-16), (cx, cy+20), (left, cy-16)], fill=body_col)

    # Accent
    d.polygon([(cx, top+12), (cx+24, top+26), (cx, top+40), (cx-24, top+26)], fill=PALETTE['team'])
    d.line([(cx-20, top+26), (cx+20, top+26)], fill=PALETTE['team_light'], width=3)

    # Class hints
    n = name
    if any(k in n for k in ['tank', 'car', 'vehicle', 'wagon']):
        d.rectangle((cx-96, cy+8, cx+96, cy+42), fill=(58, 62, 68, 255))
        d.rectangle((cx-30, cy-18, cx+88, cy+6), fill=(92, 100, 112, 255))
        d.rectangle((cx+58, cy-30, cx+130, cy-22), fill=(162, 170, 180, 255))
    if any(k in n for k in ['archer', 'crossbow', 'sniper']):
        d.line([(cx+30, cy-26), (cx+130, cy-96)], fill=(175, 184, 193, 255), width=7)
    if any(k in n for k in ['missile', 'rocket', 'sam']):
        d.polygon([(cx+48, cy-62), (cx+122, cy-94), (cx+132, cy-70), (cx+54, cy-46)], fill=(175, 183, 188, 255))
    if any(k in n for k in ['ship', 'boat', 'carrier', 'submarine', 'destroyer', 'cruiser']):
        d.polygon([(cx-120, cy+16), (cx+120, cy+16), (cx+74, cy+78), (cx-74, cy+78)], fill=(80, 92, 110, 255))
    if any(k in n for k in ['drone', 'helicopter', 'jet', 'bomber', 'balloon']):
        d.polygon([(cx, top-12), (cx+90, cy-42), (cx, cy-10), (cx-90, cy-42)], fill=(121, 129, 140, 255))

    if cls == 'resource':
        d.ellipse((cx-44, cy-18, cx+44, cy+38), fill=(99, 126, 78, 255))
        if 'gold' in n:
            d.ellipse((cx-38, cy-10, cx+38, cy+34), fill=(207, 168, 72, 255))
        if 'oil' in n or 'coal' in n:
            d.ellipse((cx-38, cy-10, cx+38, cy+34), fill=(42, 44, 50, 255))

    if cls == 'effect':
        for i in range(7):
            r = 24 + i * 24
            a = 160 - i * 20
            d.ellipse((cx-r, cy-r, cx+r, cy+r), outline=(255, 186, 88, max(30, a)), width=6)

    # outline
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    od = ImageDraw.Draw(out, 'RGBA')
    od.polygon([(cx, top), (right, cy-16), (cx, cy+20), (left, cy-16)], outline=PALETTE['outline'], width=4)
    out = out.filter(ImageFilter.GaussianBlur(0.6))
    img.alpha_composite(out)
    return img


def draw_ui_icon(name: str, kind: str, size: int = 128) -> Image.Image:
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, 'RGBA')
    cx = cy = size // 2
    d.ellipse((8, 8, size-8, size-8), fill=(41, 50, 66, 255), outline=(147, 161, 182, 255), width=5)
    accent = {
        'resource': (90, 170, 111, 255),
        'unit': (186, 104, 89, 255),
        'gameplay': (100, 144, 196, 255),
    }[kind]
    d.ellipse((18, 18, size-18, size-18), fill=accent)
    if 'food' in name:
        d.polygon([(cx, 26), (cx+14, 58), (cx, 98), (cx-14, 58)], fill=(232, 241, 168, 255))
    elif 'wood' in name:
        d.rectangle((cx-9, 30, cx+9, 96), fill=(96, 66, 44, 255))
        d.ellipse((cx-24, 18, cx+24, 54), fill=(78, 134, 72, 255))
    elif 'metal' in name:
        d.polygon([(cx, 28), (cx+28, 50), (cx+18, 88), (cx-18, 88), (cx-28, 50)], fill=(179, 186, 197, 255))
    elif 'oil' in name:
        d.ellipse((cx-16, 32, cx+16, 88), fill=(28, 32, 39, 255))
    elif 'knowledge' in name:
        d.rectangle((cx-24, 34, cx+24, 86), fill=(229, 218, 171, 255))
        d.line([(cx, 36), (cx, 84)], fill=(110, 96, 68, 255), width=3)
    elif 'wealth' in name:
        d.ellipse((cx-22, 40, cx+22, 80), fill=(215, 182, 78, 255))
    elif kind == 'unit':
        d.rectangle((cx-10, 34, cx+10, 82), fill=(234, 239, 246, 255))
        d.ellipse((cx-14, 18, cx+14, 44), fill=(234, 239, 246, 255))
        if 'tank' in name:
            d.rectangle((24, 64, 104, 86), fill=(55, 62, 70, 255))
        if 'naval' in name:
            d.polygon([(24, 78), (104, 78), (88, 102), (40, 102)], fill=(65, 80, 108, 255))
        if 'missile' in name:
            d.polygon([(70, 26), (103, 16), (100, 32), (68, 40)], fill=(206, 210, 218, 255))
    else:
        d.polygon([(cx, 24), (104, 64), (cx, 104), (24, 64)], fill=(236, 242, 252, 255))
        if 'alert' in name:
            d.rectangle((cx-4, 34, cx+4, 78), fill=(198, 62, 74, 255))
            d.ellipse((cx-4, 84, cx+4, 92), fill=(198, 62, 74, 255))
    return img


def save_group(items, subdir, draw_fn, size, cls=None):
    base = ROOT / subdir
    ensure(base)
    for n in items:
        img = draw_fn(n, size=size) if cls is None else draw_fn(n, size=size, cls=cls)
        img.save(base / f'{n}.png', 'PNG', optimize=True)


def main():
    save_group(TERRAINS, 'terrain', draw_terrain, 1024)
    save_group(ENVIRONMENT, 'environment', draw_iso_sprite, 512, cls='environment')
    save_group(RESOURCES, 'resources', draw_iso_sprite, 512, cls='resource')
    save_group(BUILDINGS_ECON, 'buildings/economy', draw_iso_sprite, 512, cls='building')
    save_group(BUILDINGS_MIL, 'buildings/military', draw_iso_sprite, 512, cls='building')
    save_group(NAVAL_BUILDINGS, 'buildings/naval', draw_iso_sprite, 512, cls='building')
    save_group(EARLY, 'units/early_era', draw_iso_sprite, 512, cls='unit')
    save_group(MEDIEVAL, 'units/medieval_era', draw_iso_sprite, 512, cls='unit')
    save_group(INDUSTRIAL, 'units/industrial_era', draw_iso_sprite, 512, cls='unit')
    save_group(MODERN, 'units/modern_era', draw_iso_sprite, 512, cls='unit')
    save_group(INFO, 'units/information_age', draw_iso_sprite, 512, cls='unit')
    save_group(STRATEGIC, 'strategic_systems', draw_iso_sprite, 512, cls='unit')
    save_group(NAVAL_UNITS, 'units/naval', draw_iso_sprite, 512, cls='unit')
    save_group(VFX, 'effects', draw_iso_sprite, 512, cls='effect')

    ensure(ROOT / 'icons/resources')
    for n in UI_RESOURCE:
        draw_ui_icon(n, 'resource', 128).save(ROOT / 'icons/resources' / f'{n}_icon.png', 'PNG', optimize=True)
    ensure(ROOT / 'icons/units')
    for n in UI_UNIT:
        draw_ui_icon(n, 'unit', 128).save(ROOT / 'icons/units' / f'{n}_icon.png', 'PNG', optimize=True)
    ensure(ROOT / 'icons/gameplay')
    for n in UI_GAMEPLAY:
        draw_ui_icon(n, 'gameplay', 128).save(ROOT / 'icons/gameplay' / f'{n}_icon.png', 'PNG', optimize=True)

    readme = ROOT / 'README.md'
    readme.write_text(
        '# Complete Stylized RTS Asset Pack\n\n'
        'Generated with `tools/generate_complete_rts_asset_pack.py`.\n\n'
        '- Terrain tiles: 1024x1024 PNG (tile-friendly).\n'
        '- Sprites/effects/buildings/units/resources: 512x512 PNG with transparent backgrounds.\n'
        '- UI icons: 128x128 PNG with transparent backgrounds.\n\n'
        'Art direction: stylized realistic RTS, clean silhouettes, soft lighting, team-color accents, strategic zoom readability.\n'
    )

    print(f'Wrote asset pack to {ROOT}')


if __name__ == '__main__':
    main()
