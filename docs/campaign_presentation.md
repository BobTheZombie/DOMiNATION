# Campaign Presentation Layer

This document describes the lightweight authored campaign/story presentation stack.

## Scope
- Briefing/debriefing presentation
- Mission message/event presentation
- Campaign progression and carryover visibility
- Objective transition feedback and debug traces

## Deterministic guarantees
- Mission/campaign authoritative state remains simulation-owned and serialized.
- Presentation assets (`portraitId`, `iconId`, `imageId`, style tags) are non-authoritative metadata only.
- Mission message queue ordering uses deterministic sequence IDs from simulation state.

## Smoke commands
- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --dump-hash`
- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 2200 --dump-hash`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 1 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 4 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 8 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --save /tmp/campaign_state.json --dump-hash`
- `./build/rts --headless --load /tmp/campaign_state.json --smoke --dump-hash`

## Fallback behavior
Portrait/icon/image references resolve to deterministic fallback IDs when missing (`ui_portrait_default` and scoped icon fallbacks). Debug HUD surfaces resolved IDs for authored validation.

## UI polish integration
- Mission messages are now surfaced in a compact chronological message log region in the command deck.
- Objective presentation is grouped by primary/secondary/hidden-revealed categories with strong visual state treatment for active/completed/failed.
- Crisis/event summaries remain deterministic in ordering and are rendered in a dedicated strategic feed.

## Campaign iconography and strategic alerts
- Briefing/debrief/event surfaces now use deterministic icon + portrait fallback IDs.
- Alert queue ordering is deterministic by severity then tick/sequence ordering.
- Objective and mission message categories carry icon IDs with stable fallback behavior.
