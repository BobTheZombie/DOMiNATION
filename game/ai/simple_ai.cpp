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



bool map_has_navigable_water(const dom::sim::World& w) {
  int water = 0;
  for (uint8_t c : w.terrainClass) if (c == (uint8_t)dom::sim::TerrainClass::ShallowWater || c == (uint8_t)dom::sim::TerrainClass::DeepWater) ++water;
  return water > (w.width * w.height) / 10;
}

bool coastal_city_exists(const dom::sim::World& w, uint16_t team) {
  for (const auto& c : w.cities) {
    if (c.team != team) continue;
    int x = std::clamp((int)c.pos.x, 0, w.width - 1);
    int y = std::clamp((int)c.pos.y, 0, w.height - 1);
    for (int oy=-2;oy<=2;++oy) for (int ox=-2;ox<=2;++ox) {
      int nx = std::clamp(x+ox,0,w.width-1), ny = std::clamp(y+oy,0,w.height-1);
      auto tc = (dom::sim::TerrainClass)w.terrainClass[ny*w.width+nx];
      if (tc != dom::sim::TerrainClass::Land) return true;
    }
  }
  return false;
}



bool find_mountain_mine_spot(const dom::sim::World& w, uint16_t team, glm::vec2& out) {
  for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
    const int cell = y * w.width + x;
    auto biome = dom::sim::biome_at(w, cell);
    if (!(biome == dom::sim::BiomeType::Mountain || biome == dom::sim::BiomeType::SnowMountain)) continue;
    if (w.territoryOwner[cell] != team) continue;
    if (!dom::sim::valid_mine_shaft_placement(w, {x, y})) continue;
    out = {x + 0.5f, y + 0.5f};
    return true;
  }
  return false;
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
  for (const auto& c : world.cities) if (c.team == team && c.capital) { base = c.pos; break; }
  uint16_t enemyTeam = team;
  for (const auto& p : world.players) { if (p.id != team && !dom::sim::players_allied(world, p.id, team) && p.alive) { enemyTeam = p.id; break; } }
  if (enemyTeam == team) {
    for (const auto& p : world.players) {
      if (p.id == team || !p.alive) continue;
      if (dom::sim::trade_access_allowed(world, team, p.id) && world.worldTension < 40.0f) {
        dom::sim::establish_trade_agreement(world, team, p.id);
        break;
      }
    }
  }
  glm::vec2 enemyBase = base + glm::vec2{20.0f,20.0f};
  for (const auto& c : world.cities) if (c.team == enemyTeam && c.capital) { enemyBase = c.pos; break; }
  glm::vec2 rally = base + glm::vec2{10.0f, 8.0f};

  const auto& civ = world.players[team].civilization;
  uint32_t cc = first_building(world, team, dom::sim::BuildingType::CityCenter);
  const int workerTarget = std::clamp((int)std::round((5.0f * civ.economyBias + 1.5f) * civ.aiWorkerTargetMult), 4, 16);
  if (cc && count_units(world, team, dom::sim::UnitType::Worker) < workerTarget) dom::sim::enqueue_train_unit(world, team, cc, dom::sim::UnitType::Worker);

  auto maybeBuild = [&](dom::sim::BuildingType t, glm::vec2 offset) {
    if (count_buildings(world, team, t) == 0) {
      dom::sim::start_build_placement(world, team, t);
      dom::sim::update_build_placement(world, team, base + offset);
      dom::sim::confirm_build_placement(world, team);
    }
  };

  if (world.players[team].popCap - world.players[team].popUsed <= 2) maybeBuild(dom::sim::BuildingType::House, {10, 5});
  glm::vec2 mountainMinePos{};
  const bool hasMountainSpot = find_mountain_mine_spot(world, team, mountainMinePos);
  if (count_buildings(world, team, dom::sim::BuildingType::Mine) == 0) {
    if (hasMountainSpot) {
      dom::sim::start_build_placement(world, team, dom::sim::BuildingType::Mine);
      dom::sim::update_build_placement(world, team, mountainMinePos);
      dom::sim::confirm_build_placement(world, team);
    } else {
      maybeBuild(dom::sim::BuildingType::Mine, {8, 8});
    }
  }

  if (civ.economyBias >= civ.militaryBias) {
    maybeBuild(dom::sim::BuildingType::Farm, {6, 4});
    maybeBuild(dom::sim::BuildingType::LumberCamp, {4, 8});
    maybeBuild(dom::sim::BuildingType::Market, {11, 9});
  } else {
    maybeBuild(dom::sim::BuildingType::Barracks, {14, 8});
    maybeBuild(dom::sim::BuildingType::LumberCamp, {4, 8});
    maybeBuild(dom::sim::BuildingType::Farm, {6, 4});
  }
  if (civ.scienceBias * civ.aiResearchPriority > 0.9f) maybeBuild(dom::sim::BuildingType::Library, {12, 12});
  maybeBuild(dom::sim::BuildingType::Barracks, {14, 8});
  if (civ.scienceBias * civ.aiReconPriority >= 1.0f || civ.defense >= 1.0f) maybeBuild(dom::sim::BuildingType::RadarTower, {9, 10});
  if (civ.militaryBias * civ.aiAirPriority >= 1.0f) maybeBuild(dom::sim::BuildingType::Airbase, {16, 10});
  if (world.worldTension > 35.0f || civ.scienceBias * civ.aiStrategicPriority > 1.05f || civ.strategicBias > 1.05f) maybeBuild(dom::sim::BuildingType::MissileSilo, {18, 12});
  if (world.worldTension > 30.0f || civ.defense > 1.0f) { maybeBuild(dom::sim::BuildingType::AABattery, {7, 13}); maybeBuild(dom::sim::BuildingType::AntiMissileDefense, {6, 15}); }

  const bool navalRelevant = map_has_navigable_water(world) && coastal_city_exists(world, team);
  if (navalRelevant) maybeBuild(dom::sim::BuildingType::Port, {9, 3});

  uint32_t barracks = first_building(world, team, dom::sim::BuildingType::Barracks);
  if (barracks) {
    int infTarget = std::clamp((int)std::round(5.0f * civ.militaryBias), 3, 12);
    int archTarget = std::clamp((int)std::round(2.0f * civ.scienceBias + 1.0f), 1, 6);
    int cavTarget = std::clamp((int)std::round(2.0f * civ.aggression + 1.0f), 1, 6);
    int siegeTarget = std::clamp((int)std::round(2.0f * civ.defense), 1, 4);
    if (count_units(world, team, dom::sim::UnitType::Infantry) < infTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Infantry);
    if (count_units(world, team, dom::sim::UnitType::Archer) < archTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Archer);
    if (count_units(world, team, dom::sim::UnitType::Cavalry) < cavTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Cavalry);
    if (count_units(world, team, dom::sim::UnitType::Siege) < siegeTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Siege);
  }

  uint32_t airbase = first_building(world, team, dom::sim::BuildingType::Airbase);
  if (airbase) {
    int fighterTarget = std::clamp((int)std::round(2.0f * civ.militaryBias + (world.worldTension > 40.0f ? 2.0f : 0.0f)), 1, 8);
    int bomberTarget = std::clamp((int)std::round(1.5f * civ.militaryBias + (civ.aggression > 1.1f ? 1.0f : 0.0f)), 1, 5);
    int reconTarget = std::clamp((int)std::round(1.0f + 2.0f * civ.scienceBias), 1, 4);
    if (count_units(world, team, dom::sim::UnitType::Fighter) < fighterTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Fighter);
    if (count_units(world, team, dom::sim::UnitType::Interceptor) < fighterTarget / 2) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Interceptor);
    if (count_units(world, team, dom::sim::UnitType::Bomber) < bomberTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Bomber);
    if (count_units(world, team, dom::sim::UnitType::ReconDrone) < reconTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::ReconDrone);
  }
  uint32_t silo = first_building(world, team, dom::sim::BuildingType::MissileSilo);
  if (silo && world.worldTension > 50.0f && civ.scienceBias >= 1.0f) {
    if (count_units(world, team, dom::sim::UnitType::TacticalMissile) < 2) dom::sim::enqueue_train_unit(world, team, silo, dom::sim::UnitType::TacticalMissile);
    if (count_units(world, team, dom::sim::UnitType::StrategicMissile) < 1) dom::sim::enqueue_train_unit(world, team, silo, dom::sim::UnitType::StrategicMissile);
  }

  uint32_t lib = first_building(world, team, dom::sim::BuildingType::Library);
  if (lib && world.players[team].age < dom::sim::Age::Classical && civ.scienceBias >= 0.9f) dom::sim::enqueue_age_research(world, team, lib);

  uint32_t port = first_building(world, team, dom::sim::BuildingType::Port);
  if (port && navalRelevant) {
    int tTarget = std::max(1, (int)std::round(civ.economyBias));
    int lTarget = std::max(1, (int)std::round(civ.militaryBias));
    int hTarget = std::max(1, (int)std::round(civ.aggression));
    int bTarget = std::max(1, (int)std::round(civ.defense));
    if (count_units(world, team, dom::sim::UnitType::TransportShip) < tTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::TransportShip);
    if (count_units(world, team, dom::sim::UnitType::LightWarship) < lTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::LightWarship);
    if (count_units(world, team, dom::sim::UnitType::HeavyWarship) < hTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::HeavyWarship);
    if (count_units(world, team, dom::sim::UnitType::BombardShip) < bTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::BombardShip);
  }

  std::vector<uint32_t> army;
  for (const auto& u : world.units) if (u.team == team && u.type != dom::sim::UnitType::Worker) army.push_back(u.id);
  std::sort(army.begin(), army.end());
  if (army.empty()) return;

  for (const auto& s : world.guardianSites) {
    if (s.discovered || !s.siteActive || s.siteDepleted) continue;
    if (glm::length(s.pos - base) < 45.0f) {
      dom::sim::issue_move(world, team, army, s.pos);
      ++world.aiDecisionCount;
      return;
    }
  }

  float tensionFactor = std::clamp(1.0f - world.worldTension / 180.0f, 0.5f, 1.2f);
  const float expansionTiming = std::max(0.7f, civ.aiExpansionTiming);
  int attackThreshold = std::clamp((int)std::round((((gAttackEarly ? 5.0f : 8.0f) * expansionTiming) / std::max(0.7f, civ.aggression)) * tensionFactor), 3, 12);
  if ((int)army.size() < attackThreshold) { ++world.aiDecisionCount; return; }

  TeamStrength ours = strength_near(world, team, enemyBase, 30.0f);
  TeamStrength theirs = strength_near(world, enemyTeam, enemyBase, 30.0f);

  dom::sim::OperationType opType = dom::sim::OperationType::RallyAndPush;
  glm::vec2 opTarget = enemyBase;
  for (const auto& op : world.operations) {
    if (op.team == team && op.active) { opType = op.type; opTarget = op.target; break; }
  }
  if (opType == dom::sim::OperationType::DefendBorder || opType == dom::sim::OperationType::SecureRoute) opTarget = rally;

  const int retreatHpThreshold = (int)std::round((gAggressive ? 240.0f : 360.0f) * std::max(0.75f, civ.defense));
  int outSupply = 0; for (const auto& u : world.units) if (u.team == team && u.type != dom::sim::UnitType::Worker && u.supplyState == dom::sim::SupplyState::OutOfSupply) ++outSupply;
  const bool shouldRetreat = ours.hp < retreatHpThreshold || (theirs.hp > 0 && ours.hp * 100 < theirs.hp * (gAggressive ? 60 : 80)) || outSupply > (int)army.size() / 3;

  if (shouldRetreat) {
    dom::sim::issue_move(world, team, army, rally);
    ++world.aiRetreatCount;
  } else {
    if (opType == dom::sim::OperationType::RaidEconomy) dom::sim::issue_move(world, team, army, opTarget);
    else dom::sim::issue_attack_move(world, team, army, opTarget);
  }

  ++world.aiDecisionCount;
}
}
