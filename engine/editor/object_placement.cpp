#include "engine/editor/object_placement.h"

namespace dom::editor {
namespace {
uint32_t next_id(uint32_t currentMax) { return currentMax + 1; }
}

void place_editor_object(dom::sim::World& world, int tool, uint16_t owner, const glm::vec2& at) {
  if (tool == 0) {
    uint32_t last = world.cities.empty() ? 0 : world.cities.back().id;
    world.cities.push_back({next_id(last), owner, at, 1, false});
  } else if (tool == 1) {
    uint32_t last = world.buildings.empty() ? 0 : world.buildings.back().id;
    world.buildings.push_back({next_id(last), owner, dom::sim::BuildingType::Barracks, at, {3.0f, 3.0f}, false, 1.0f, 14.0f, 1000.0f, 1000.0f, {}});
  } else if (tool == 2) {
    uint32_t last = world.resourceNodes.empty() ? 0 : world.resourceNodes.back().id;
    world.resourceNodes.push_back({next_id(last), dom::sim::ResourceNodeType::Ore, at, 1200.0f, owner});
  } else if (tool == 3) {
    uint32_t last = world.units.empty() ? 0 : world.units.back().id;
    dom::sim::Unit u{};
    u.id = next_id(last);
    u.team = owner;
    u.type = dom::sim::UnitType::Infantry;
    u.role = dom::sim::UnitRole::Infantry;
    u.pos = u.renderPos = u.target = u.slotTarget = at;
    world.units.push_back(u);
  }
}
} // namespace dom::editor
