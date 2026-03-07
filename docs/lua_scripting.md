# Lua scenario scripting (safe deterministic hooks)

- Lua execution is trigger-driven only (`RunLuaHook`).
- Exposed API: `activate_objective`, `complete_objective`, `fail_objective`, `show_message`, `get_tick`, `get_objective_state`, `get_player_alive`, `get_world_tension`.
- `os`, `io`, and `package` are disabled for sandbox safety.
- Lua orchestrates mission content only; authoritative sim stays in C++.


Lua hooks continue to run in sandbox; civilization mission tags are available through player civilization data and remain deterministic because hook execution log and mission runtime counters are serialized in authority saves.

## Campaign-safe Lua hooks

Available campaign APIs at deterministic mission hook points: `get_campaign_flag`, `set_campaign_flag`, `get_campaign_resource`, `add_campaign_resource`, `get_previous_mission_result`, `set_campaign_branch`, `unlock_campaign_reward`, `get_campaign_variable`.
