#include "engine/editor/object_placement.h"
#include <cmath>

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
  } else if (tool == 4) {
    uint32_t nid = world.railNodes.empty() ? 1u : world.railNodes.back().id + 1;
    dom::sim::RailNodeType type = dom::sim::RailNodeType::Station;
    world.railNodes.push_back({nid, owner, type, {(int)std::round(at.x), (int)std::round(at.y)}, 0u, true});
    if (world.railNodes.size() >= 2) {
      const auto& prev = world.railNodes[world.railNodes.size() - 2];
      if (prev.owner == owner) {
        uint32_t eid = world.railEdges.empty() ? 1u : world.railEdges.back().id + 1;
        world.railEdges.push_back({eid, owner, prev.id, nid, 1, false, false, false});
      }
    }
  }
}
} // namespace dom::editor
