# Strategic Alerts

Strategic alerts are generated from mission messages, world tension, Armageddon state, and gameplay notifications.

## Severity tiers
- info
- warning
- critical
- apocalyptic

## Deterministic ordering
Alerts are sorted by severity, then deterministic `orderKey` derived from tick/sequence data.

## Integration
Alerts are rendered in the HUD strategic alert panel and share icon resolution/fallback behavior with mission/event UI.
