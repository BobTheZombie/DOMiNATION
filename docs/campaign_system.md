# Campaign System

The campaign layer orchestrates deterministic multi-mission progression using a top-level campaign JSON file (`--campaign <file>`).

## File format

`campaigns/test_campaign.json` demonstrates:
- `campaign_id`, `display_name`, `description`
- `starting_state` carryover values (`player_civilization`, `unlocked_age`, resources, flags, variables)
- `missions[]` entries with `mission_id`, `scenario`, `briefing`, optional `debrief` and `intro_image`
- `prerequisites`
- `next` mapping by result (`victory`, `defeat`, `partial_victory`, custom)
- `next_by_branch` mapping for scripted branch keys.

## Carryover model

Carryover is compact and deterministic:
- player civilization id
- unlocked age
- bounded resource carryover
- veteran unit ids (lightweight references)
- discovered guardian ids
- world tension
- rewards, flags, and integer variables
- previous mission result and pending branch key.

## Progression

On mission end, campaign runtime captures result and updates carryover. Next mission is selected from:
1. `next_by_branch[pending_branch]` (if set)
2. `next[result_tag]`

An empty next mission ends the campaign as complete (victory/partial) or failed (defeat).

## Save/load

Campaign runtime save uses `--save` with a campaign run. It stores mission progression + carryover and reloads via `--load`.

## Deterministic smoke commands

- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --dump-hash`
- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 2200 --dump-hash`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 1 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 4 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --threads 8 --hash-only`
- `./build/rts --headless --campaign campaigns/test_campaign.json --smoke --ticks 1200 --save /tmp/campaign_state.json --dump-hash`
- `./build/rts --headless --load /tmp/campaign_state.json --smoke --dump-hash`
