# Lua scenario scripting (safe deterministic hooks)

Lua hooks are deterministic orchestration only. Core simulation authority remains in C++.

## Execution model

- Lua only runs at deterministic trigger points (`RunLuaHook` trigger action).
- No background coroutines/timers owned by Lua.
- Hook invocations are logged in authoritative mission runtime state.

## Sandbox

- Disabled globals/libraries: `io`, `os`, `package`, `debug`, `dofile`, `loadfile`, `require`.
- `math.random` and `math.randomseed` are removed to avoid nondeterministic script entropy.
- Scripts cannot access engine internals directly; they call a curated API surface.

## Scenario hook sources

Mission data supports either:

- `mission.luaScript`: file-backed script path, or
- `mission.luaInline`: inline Lua source.

## Safe API

Objective/message APIs:

- `activate_objective(id)`
- `complete_objective(id)`
- `fail_objective(id)`
- `show_message(text)`

Mission/sim read APIs:

- `get_tick()`
- `get_objective_state(id)`
- `get_player_alive(id)`
- `get_world_tension()`

Validated mutation APIs:

- `spawn_unit(type, x, y, owner)`
- `spawn_building(type, x, y, owner)`
- `grant_resource(player, type, amount)`
- `set_relation(a, b, state)`
- `launch_operation(type, player, target_x, target_y)`
- `reveal_area(player, x, y, radius)`

Campaign-oriented APIs (deterministic state only):

- `get_campaign_flag(name)` / `set_campaign_flag(name, value)`
- `get_campaign_resource(name)` / `add_campaign_resource(name, amount)`
- `get_previous_mission_result()`
- `set_campaign_branch(branch_key)`
- `unlock_campaign_reward(reward_id)`
- `get_campaign_variable(name)`

Guardian mission helpers (existing campaign scripting surface):

- `activate_guardian_site(site_instance_id)`
- `reveal_guardian_site(site_instance_id)`
- `assign_guardian_owner(site_instance_id, player)`


## Strategic deterrence note
Deterrence data is currently not writable from Lua; scenario data and simulation rules drive authoritative strategic posture/readiness.

## Campaign presentation hooks
- `show_message(text)` now feeds deterministic mission message ordering plus objective/event log output.
- Recommended message convention for authored scripts: prefix text with category markers (`[briefing]`, `[intelligence]`, `[warning]`) when needed.


World event helpers:
- `get_world_event_state(event_id)` returns `"inactive"|"active"|"resolved"`.
- `trigger_world_event(event_id)` activates a data-driven crisis from `content/world_events.json` deterministically.
