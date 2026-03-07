# Mountain Mining System

## Deterministic mountain layer
- Added `mountain` and `snow_capped_mountains` biome support.
- Snow caps are generated from deterministic elevation + temperature checks.
- Mountain regions are grouped into stable region IDs and stored in authoritative state.

## Deposits
- Surface deposits: moderate ore nodes visible on mountain slopes.
- Deep deposits: richer nodes tied to underground graph nodes.
- Deep deposits are serialized (`deepDeposits`) and hashed for replay parity.

## Underground graph
- Each mountain region owns deterministic underground nodes and edges.
- Node types: shaft, deposit, depot, junction/exit (future extension).
- Edge set is deterministic and serialized (`undergroundNodes`, `undergroundEdges`).

## Logistics and extraction
- Mines on valid mountain cells operate as mine entrances + shafts.
- Connected tunnel graphs increase deep extraction rate.
- Disconnected networks extract at reduced efficiency.

## Perf counters
- `MOUNTAIN_REGION_COUNT`
- `SURFACE_DEPOSIT_COUNT`
- `DEEP_DEPOSIT_COUNT`
- `ACTIVE_MINE_SHAFTS`
- `ACTIVE_TUNNELS`
- `UNDERGROUND_DEPOTS`
- `UNDERGROUND_YIELD`
