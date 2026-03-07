# Objective and mission runtime model

Objectives support category (`primary`, `secondary`, `hidden_optional`), owner, visibility, progress text/value, and state (`inactive`, `active`, `completed`, `failed`).

Mission runtime tracks briefing visibility, status, result tags, active objectives, fired trigger counters, and lua hook log.


Save/load parity notes: objective identity (`objective_id`), visibility/progress presentation, active state, trigger fire state, and mission runtime counters are serialized on authoritative saves used by campaign continuation.
