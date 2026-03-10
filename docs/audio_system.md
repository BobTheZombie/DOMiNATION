# Deterministic audio system

This pass adds a bounded audio integration layer that is **presentation-only** and non-authoritative.

## Model
- `engine/audio/audio_system.*` owns runtime audio state/settings and trigger APIs.
- `engine/audio/audio_resolution.*` resolves event keys through deterministic manifest order:
  1) exact event mapping
  2) civilization/theme mapping
  3) category mapping
  4) default mapping
  5) silent fallback
- `content/audio_manifest.json` defines sound IDs and fallback chains.

Audio playback success/failure never changes simulation state or hash output.

## Deterministic trigger reconstruction
- UI/command sounds are triggered from deterministic input + stable IDs/ticks.
- Event sounds are triggered from existing gameplay events/counters (`consume_gameplay_events`, strategic/crisis/guardian counters).
- Ambient playback is a bounded context pass keyed by world state + camera zoom and tick cadence.

## Runtime controls
- Audio enabled
- Master volume
- UI volume
- World/effects volume
- Ambient volume

## Debug visibility
- `AUDIO_RESOLVE_COUNT`
- `AUDIO_FALLBACK_COUNT`
- `ACTIVE_AMBIENT_CHANNELS`
- `EVENT_SOUND_TRIGGERS`
- `UI_SOUND_TRIGGERS`
- Missing audio reports are exposed only through debug HUD mode.

## Determinism smoke commands
```bash
./build/rts --headless --smoke --ticks 400 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 800 --dump-hash
./build/rts --headless --scenario scenarios/world_events_test.json --smoke --ticks 1200 --dump-hash
./build/rts --headless --scenario scenarios/armageddon_test.json --smoke --ticks 1800 --dump-hash
./build/rts --headless --scenario scenarios/mythic_guardians_multi_test.json --smoke --ticks 1600 --dump-hash
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only
./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only
```
