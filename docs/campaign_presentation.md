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
