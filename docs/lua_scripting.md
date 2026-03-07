# Lua scenario scripting (safe deterministic hooks)

- Lua execution is trigger-driven only (`RunLuaHook`).
- Exposed API: `activate_objective`, `complete_objective`, `fail_objective`, `show_message`, `get_tick`, `get_objective_state`, `get_player_alive`, `get_world_tension`.
- `os`, `io`, and `package` are disabled for sandbox safety.
- Lua orchestrates mission content only; authoritative sim stays in C++.
