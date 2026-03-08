# Dynamic Ideology and Alliance Bloc System

This system adds deterministic ideology-driven alliance blocs.

- Civilizations define `ideology.primary`, `ideology.secondary`, ideology weights, and bloc affinity/hostility weights in `content/civilizations.json`.
- Bloc templates are data-driven in `content/alliance_blocs.json`.
- Runtime authoritative state is held in `World::allianceBlocs` and included in save/load + deterministic state hashing.
- Blocs affect diplomacy persistence, trade throughput, and rival-bloc hostility pressure.
- Under Armageddon, bloc cohesion is reduced and members may leave if survival pressure dominates.

Smoke scenario: `scenarios/bloc_test.json`.
