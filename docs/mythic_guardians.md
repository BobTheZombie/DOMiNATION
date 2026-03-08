# Mythic Guardians

Mythic Guardians are deterministic, authoritative biome encounters.

## Implemented guardians

- Snow Yeti: snow mountain lair, joins discoverer.
- Kraken: deep-ocean trench hostile naval guardian.
- Sandworm: desert dune nest hostile ambush guardian.
- Forest Spirit: forest/jungle sacred grove conditional ally (discoverer control by default).

## Authoritative runtime state

Serialized + hashed fields include guardian definitions, site instances, discovery/spawn/alive/owner/depletion/behavior flags, and counters:

- `discovered`, `spawned`, `joined`, `killed`
- `hostile_events`, `allied_events`

Runtime caches (visual-only/debug-only) are excluded from guardian authority.

## Scenario fields

`mythicGuardians.sites[]` supports `guardian_id`, `site_type`, `pos`, and optional runtime state including `scenario_placed`.

`mythicGuardians.counters` supports `discovered`, `spawned`, `joined`, `killed`, `hostile_events`, `allied_events`.

See: `scenarios/mythic_guardians_test.json`, `scenarios/mythic_guardians_multi_test.json`.

## Lua hooks

- `activate_guardian_site(instanceId)`
- `reveal_guardian_site(instanceId, discovererPlayer)`
- `assign_guardian_owner(instanceId, player)`

## Deterministic validation commands

- `./build/rts --headless --smoke --ticks 1800 --seed 1234 --dump-hash`
- `./build/rts --headless --scenario scenarios/mythic_guardians_test.json --smoke --ticks 2200 --dump-hash`
- `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 2400 --dump-hash`
- `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 1 --hash-only`
- `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 4 --hash-only`
- `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --threads 8 --hash-only`
- `./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 1200 --save /tmp/guardian_save.json --dump-hash`
- `./build/rts --headless --load /tmp/guardian_save.json --smoke --ticks 2400 --dump-hash`

## Presentation references
Guardians may now include deterministic presentation IDs (`icon_id`, `portrait_id`, `site_icon_id`, `site_label_id`) in content. Runtime resolves by guardian ID first, then site type, then stable fallback IDs.
