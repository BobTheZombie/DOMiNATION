# Rail Logistics System

This document describes the deterministic industrial rail logistics layer.

## Authoritative model
- Rail nodes: station/junction/depot.
- Rail edges: owner, endpoint node ids, quality, bridge/tunnel, disruption flag.
- Rail networks: deterministic owner-local connected components.
- Trains: supply/freight (+ armored hook) with deterministic route/cursor/progress/cargo/state.

## Deterministic behavior
- Train movement runs on fixed simulation ticks (`tick % 5 == 0`).
- Route search is deterministic BFS over authoritative rail edges.
- Disrupted edges invalidate movement and route activity deterministically.
- Rail counters are part of authoritative state hash.

## Logistics effects
- Supply trains reduce effective supply distance for connected armies.
- Freight trains increase metal/wealth throughput for owner economy.
- Disruption lowers active throughput and increments disruption counters.

## Debug/perf counters
- `RAIL_NODE_COUNT`
- `RAIL_EDGE_COUNT`
- `ACTIVE_RAIL_NETWORKS`
- `ACTIVE_TRAINS`
- `ACTIVE_SUPPLY_TRAINS`
- `ACTIVE_FREIGHT_TRAINS`
- `RAIL_THROUGHPUT`
- `DISRUPTED_RAIL_ROUTES`

## Smoke validation
Use the commands listed in README for baseline, scenario, thread parity, and save/load parity checks.
