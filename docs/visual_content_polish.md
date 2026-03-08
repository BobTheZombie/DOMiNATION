# Visual/content polish pass (deterministic)

This pass improves biome readability, civilization visual coherence, industrial/rail/strategic overlays, guardian/site presentation, and UI fallback clarity.

## Deterministic presentation model

- Presentation resolves from deterministic IDs only (biome ID, civilization theme ID, unit/building definition ID, guardian ID/site type, event ID/category).
- Missing content always resolves to a stable fallback ID (`ui_icon_*_fallback`, `ui_portrait_default`).
- Presentation lookups are non-authoritative; gameplay state/hash logic is unchanged.

## Added/extended mappings

- `content/biomes.json` and `content/biome_manifest.json`: refined `color_palette_hint` values for all core biomes.
- `content/civilization_themes.json`: added industrial/strategic building-family variant mappings for Rome/China/Europe/Middle East.
- `content/mythic_guardians.json`: optional `presentation` object (`icon_id`, `portrait_id`, `site_icon_id`, `site_label_id`).

## Debug/perf counters

- `CONTENT_FALLBACK_COUNT`
- `CIV_PRESENTATION_RESOLVES`
- `GUARDIAN_PRESENTATION_RESOLVES`
- `CAMPAIGN_PRESENTATION_RESOLVES`
- `EVENT_PRESENTATION_RESOLVES`

These are emitted in `--perf` logs.

## Smoke commands

```bash
./build/rts --headless --smoke --ticks 400 --seed 1234 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 600 --dump-hash
./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```
