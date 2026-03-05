# Vertical Slice Design

- Map: deterministic procedural 128x128 height/fertility grid.
- Modes:
  - RTS mode with pan/zoom, selection, build placement, training and research toggles.
  - GOD mode (`G`) disables fog and raises zoom cap.
- Resources: Food, Wood, Metal, Wealth, Knowledge, Oil.
- Core loop:
  1. Place economic buildings.
  2. Complete construction.
  3. Train workers/infantry from queues.
  4. Expand pop cap via houses.
  5. Research age-up.
  6. Assemble army and attack.
- Buildings in slice:
  - CityCenter, House, Farm, LumberCamp, Mine, Market, Library, Barracks.
- Units in slice:
  - Worker, Infantry.
- AI: deterministic planner-like build order for economy -> barracks -> army -> attack.
- Victory:
  - Score winner at 10-minute limit (conquest hooks remain minimal).
