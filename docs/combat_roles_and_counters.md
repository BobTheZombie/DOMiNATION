# Combat Roles and Counters

This pass emphasizes legible counters without hard lock-in.

## Core interactions

- Infantry anchors lines and pressures exposed ranged/support.
- Cavalry raids and punishes exposed ranged/siege/recon support.
- Siege punishes buildings and static concentrations but is vulnerable to fast pressure.
- Naval escorts punish transports and protect bombard/capital elements.
- Interceptors and fighters provide explicit anti-air pressure.

## Counter readability implementation

- Role labels, role purpose text, and counter hints are exposed for selected units and production cards.
- Role-vs-role multipliers were tuned for infantry/ranged/cavalry/siege readability.
- A deterministic unit-type counter multiplier layer clarifies anti-air and escort interactions.

## Telemetry

Headless output now includes:

- first military contact tick
- first artillery tick
- first armor tick (cavalry role proxy)
- first air unit tick
- role mix snapshot
- casualty mix by role buckets
- dominant composition snapshot by phase-output buckets
