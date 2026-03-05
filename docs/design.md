# Vertical Slice Design

- Map: deterministic procedural 128x128 height/fertility grid.
- Modes:
  - RTS mode with pan/zoom, selection, right-click move.
  - GOD mode (`G`) disables fog and raises zoom cap.
- Resources: Food, Wood, Metal, Wealth, Knowledge, Oil.
- Ages: 8 total; progression gated by Knowledge + Wealth thresholds.
- Victory:
  - Conquest via capital removal hook.
  - Score winner at 10-minute limit.
- AI: simple wave attacker for skirmish.
