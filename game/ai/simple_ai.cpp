#include "game/ai/simple_ai.h"
#include <vector>

namespace dom::ai {

namespace {
uint32_t first_building(const dom::sim::World& w, uint16_t team, dom::sim::BuildingType t) {
  for (const auto& b : w.buildings) if (b.team == team && b.type == t && !b.underConstruction) return b.id;
  return 0;
}
int count_units(const dom::sim::World& w, uint16_t team, dom::sim::UnitType t) {
  int n = 0; for (const auto& u : w.units) if (u.team == team && u.type == t) ++n; return n;
}
int count_buildings(const dom::sim::World& w, uint16_t team, dom::sim::BuildingType t) {
  int n = 0; for (const auto& b : w.buildings) if (b.team == team && b.type == t && !b.underConstruction) ++n; return n;
}
}

void update_simple_ai(dom::sim::World& world, uint16_t team) {
  if (world.tick % 20 != 0) return;

  glm::vec2 base = team == 0 ? glm::vec2{20, 20} : glm::vec2{95, 95};

  uint32_t cc = first_building(world, team, dom::sim::BuildingType::CityCenter);
  if (cc && count_units(world, team, dom::sim::UnitType::Worker) < 6) {
    dom::sim::enqueue_train_unit(world, team, cc, dom::sim::UnitType::Worker);
  }

  auto maybeBuild = [&](dom::sim::BuildingType t, glm::vec2 offset) {
    if (count_buildings(world, team, t) == 0) {
      dom::sim::start_build_placement(world, team, t);
      dom::sim::update_build_placement(world, team, base + offset);
      dom::sim::confirm_build_placement(world, team);
    }
  };

  maybeBuild(dom::sim::BuildingType::Farm, {6, 4});
  maybeBuild(dom::sim::BuildingType::LumberCamp, {4, 8});
  maybeBuild(dom::sim::BuildingType::Mine, {8, 8});
  if (world.players[team].popCap - world.players[team].popUsed <= 2) maybeBuild(dom::sim::BuildingType::House, {10, 5});
  maybeBuild(dom::sim::BuildingType::Market, {11, 9});
  maybeBuild(dom::sim::BuildingType::Library, {12, 12});
  maybeBuild(dom::sim::BuildingType::Barracks, {14, 8});

  uint32_t barracks = first_building(world, team, dom::sim::BuildingType::Barracks);
  if (barracks && count_units(world, team, dom::sim::UnitType::Infantry) < 10) {
    dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Infantry);
  }

  uint32_t lib = first_building(world, team, dom::sim::BuildingType::Library);
  if (lib && world.players[team].age < dom::sim::Age::Classical) {
    dom::sim::enqueue_age_research(world, team, lib);
  }

  if (count_units(world, team, dom::sim::UnitType::Infantry) >= 8) {
    std::vector<uint32_t> mine;
    for (const auto& u : world.units) if (u.team == team && u.type == dom::sim::UnitType::Infantry) mine.push_back(u.id);
    if (!mine.empty()) dom::sim::issue_move(world, team, mine, team == 0 ? glm::vec2{95, 95} : glm::vec2{20, 20});
  }

  ++world.aiDecisionCount;
}
}
