# Trigger/objective format

Objective:
- `id`, `title`, `text`, `primary`, `owner`, `state` (`inactive|active|completed|failed`).

Trigger:
- `id`, `once`
- `condition` block with type:
  - `TickReached` (`tick`)
  - `EntityDestroyed` (`entityId`)
  - `BuildingCompleted` (`buildingType`, optional `player`)
  - `AreaEntered` (`areaId`, optional `player`)
  - `PlayerEliminated` (`player`)
- `actions` array:
  - `ShowObjectiveText` (`text`)
  - `SetObjectiveState` (`objectiveId`, `state`)
  - `GrantResources` (`player`, `resources`)
  - `SpawnUnits` (`player`, `unitType`, `count`, `pos`)
  - `EndMatchWithVictory` (`winner`)
  - `EndMatchWithDefeat` (`winner`)
  - `RevealArea` (`areaId`)

Evaluation is deterministic and stable by trigger `id` each tick.
