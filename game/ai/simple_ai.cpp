#include "game/ai/simple_ai.h"
#include <algorithm>
#include <vector>
#include <glm/geometric.hpp>

namespace dom::ai {

namespace {
bool gAttackEarly = false;
bool gAggressive = false;

struct TeamStrength { int hp{0}; int units{0}; };

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

TeamStrength strength_near(const dom::sim::World& w, uint16_t team, glm::vec2 p, float r) {
  TeamStrength s{};
  for (const auto& u : w.units) {
    if (u.team != team || u.hp <= 0) continue;
    if (glm::length(u.pos - p) > r) continue;
    s.hp += static_cast<int>(u.hp);
    ++s.units;
  }
  return s;
}
}

void set_attack_early(bool enabled) { gAttackEarly = enabled; }
void set_aggressive(bool enabled) { gAggressive = enabled; }

void update_simple_ai(dom::sim::World& world, uint16_t team) {
  if (!dom::sim::gameplay_orders_allowed(world)) return;
  if (world.tick % 20 != 0) return;

  glm::vec2 base = team == 0 ? glm::vec2{20, 20} : glm::vec2{95, 95};
  glm::vec2 enemyBase = team == 0 ? glm::vec2{95, 95} : glm::vec2{20, 20};
  glm::vec2 rally = team == 0 ? glm::vec2{31, 30} : glm::vec2{84, 84};

  uint32_t cc = first_building(world, team, dom::sim::BuildingType::CityCenter);
  if (cc && count_units(world, team, dom::sim::UnitType::Worker) < 6) dom::sim::enqueue_train_unit(world, team, cc, dom::sim::UnitType::Worker);

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
  if (barracks) {
    if (count_units(world, team, dom::sim::UnitType::Infantry) < 6) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Infantry);
    if (count_units(world, team, dom::sim::UnitType::Archer) < 3) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Archer);
    if (count_units(world, team, dom::sim::UnitType::Cavalry) < 3) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Cavalry);
    if (count_units(world, team, dom::sim::UnitType::Siege) < 2) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Siege);
  }

  uint32_t lib = first_building(world, team, dom::sim::BuildingType::Library);
  if (lib && world.players[team].age < dom::sim::Age::Classical) dom::sim::enqueue_age_research(world, team, lib);

  std::vector<uint32_t> army;
  for (const auto& u : world.units) if (u.team == team && u.type != dom::sim::UnitType::Worker) army.push_back(u.id);
  std::sort(army.begin(), army.end());
  if (army.empty()) return;

  const int attackThreshold = gAttackEarly ? 5 : 8;
  if ((int)army.size() < attackThreshold) { ++world.aiDecisionCount; return; }

  TeamStrength ours = strength_near(world, team, enemyBase, 30.0f);
  TeamStrength theirs = strength_near(world, static_cast<uint16_t>(1 - team), enemyBase, 30.0f);

  const int retreatHpThreshold = gAggressive ? 240 : 360;
  const bool shouldRetreat = ours.hp < retreatHpThreshold || (theirs.hp > 0 && ours.hp * 100 < theirs.hp * (gAggressive ? 60 : 80));

  if (shouldRetreat) {
    dom::sim::issue_move(world, team, army, rally);
    ++world.aiRetreatCount;
  } else {
    dom::sim::issue_attack_move(world, team, army, enemyBase);
  }

  ++world.aiDecisionCount;
}
}
