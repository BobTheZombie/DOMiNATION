# Territory Rendering System

## Scope
This system implements territory and border visualization only. It must not alter authoritative gameplay behavior.

## Inputs
- `World::territoryOwner`
- diplomacy relation helpers (`players_allied`, `players_at_war`)
- city/theater/site metadata
- world event + Armageddon state

## Outputs
- ownership readability tint
- civ-distinct border accents
- contested/frontline/crisis overlays
- strategic label hooks
- minimap territory coherence markers

## Deterministic design
- fixed-order tile traversal for overlays and minimap extraction
- relation color mapping from deterministic civ/team IDs
- no random sampling or frame-local nondeterministic state

## Authority contract
- renderer computes derived visuals from world state each frame
- no simulation writes, no save payload dependencies
- authoritative hashes remain unchanged when render toggles differ

## Validation suite
Run:
- `./build/rts --headless --smoke --ticks 400 --dump-hash`
- `./build/rts --headless --scenario scenarios/civ_content_test.json --smoke --ticks 800 --dump-hash`
- `./build/rts --headless --scenario scenarios/theater_operations_test.json --smoke --ticks 1200 --dump-hash`
- `./build/rts --headless --scenario scenarios/civ_content_test.json --threads 1 --hash-only`
- `./build/rts --headless --scenario scenarios/civ_content_test.json --threads 4 --hash-only`
- `./build/rts --headless --scenario scenarios/civ_content_test.json --threads 8 --hash-only`
