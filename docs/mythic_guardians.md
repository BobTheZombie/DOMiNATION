# Mythic Guardians

Mythic Guardians are a deterministic, authoritative world layer for legendary biome-tied encounters.

## Data model

- Content file: `content/mythic_guardians.json`
- Definition fields:
  - `guardian_id`, `display_name`
  - `biome_requirement`, `site_type`
  - `spawn_mode`, `max_per_map`, `unique`
  - `discovery_mode`, `behavior_mode`, `join_mode`
  - `associated_unit_definition`
  - optional `reward_hook`, `effect_hook`
  - `scenario_only`, `procedural`
  - procedural tuning (`rarity_permille`, `min_spacing_cells`, `discovery_radius`)
  - optional unit stats override (`hp`, `attack`, `range`, `speed`)

Runtime authoritative site state:
- `instanceId`, `guardianId`, `siteType`
- `pos`, `regionId`, `nodeId`
- `discovered`, `alive`, `owner`
- `siteActive`, `siteDepleted`, `spawned`
- `behaviorState`, `cooldownTicks`, `oneShotUsed`

## Snow Yeti

- `guardian_id`: `snow_yeti`
- Unique per map.
- Site: `yeti_lair` (and framework supports `frozen_cavern`).
- Discovers by proximity and deterministic reveal paths.
- Spawns on discovery and joins discoverer (`discoverer_control`).
- Implemented as a strong, slow, high-HP melee/shock unit profile.
- No respawn after death (`oneShotUsed` and depleted site).

## Scenario schema

Scenarios can author mythic sites with:

```json
"mythicGuardians": {
  "sites": [
    {
      "instance_id": 1,
      "guardian_id": "snow_yeti",
      "site_type": "yeti_lair",
      "pos": [38.0, 36.0],
      "discovered": false,
      "spawned": false
    }
  ],
  "counters": {
    "discovered": 0,
    "spawned": 0,
    "joined": 0,
    "killed": 0
  }
}
```

## Determinism and authoritative state

Included in authoritative hash/save/load:
- definition/runtime site data
- discovery/spawn/owner/alive state
- guardian counters (`discovered`, `spawned`, `joined`, `killed`)

Non-authoritative:
- presentation-only visuals and overlays.

## Lua hooks

- `activate_guardian_site(instanceId)`
- `reveal_guardian_site(instanceId, discovererPlayer)`
- `assign_guardian_owner(instanceId, player)`

## Extension path

Data stubs included for:
- Kraken (`abyssal_trench`)
- Sandworm (`dune_nest`)
- Forest Spirit (`sacred_grove`)

Add future guardians by content-first definitions plus optional behavior handling.
