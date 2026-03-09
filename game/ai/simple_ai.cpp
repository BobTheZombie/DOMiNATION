#include "game/ai/simple_ai.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <glm/geometric.hpp>

namespace dom::ai {

namespace {
bool gAttackEarly = false;
bool gAggressive = false;

struct TeamStrength { int hp{0}; int units{0}; };
struct ArmyMix {
  int armor{0};
  int air{0};
  int staticBreak{0};
  int rangedSupport{0};
};

uint32_t first_building(const dom::sim::World& w, uint16_t team, dom::sim::BuildingType t) {
  for (const auto& b : w.buildings) if (b.team == team && b.type == t && !b.underConstruction) return b.id;
  return 0;
}
int count_units(const dom::sim::World& w, uint16_t team, dom::sim::UnitType t) {
  int n = 0; for (const auto& u : w.units) if (u.team == team && u.type == t && u.hp > 0.0f) ++n; return n;
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

ArmyMix scan_enemy_mix(const dom::sim::World& w, uint16_t team) {
  ArmyMix mix{};
  for (const auto& u : w.units) {
    if (u.team != team || u.hp <= 0.0f) continue;
    const auto t = u.type;
    if (t == dom::sim::UnitType::Cavalry || t == dom::sim::UnitType::HeavyWarship || t == dom::sim::UnitType::BombardShip) ++mix.armor;
    if (t == dom::sim::UnitType::Fighter || t == dom::sim::UnitType::Interceptor || t == dom::sim::UnitType::Bomber ||
        t == dom::sim::UnitType::StrategicBomber || t == dom::sim::UnitType::ReconDrone || t == dom::sim::UnitType::StrikeDrone) ++mix.air;
    if (t == dom::sim::UnitType::Siege || t == dom::sim::UnitType::BombardShip || t == dom::sim::UnitType::StrategicMissile || t == dom::sim::UnitType::TacticalMissile) ++mix.staticBreak;
    if (t == dom::sim::UnitType::Archer || t == dom::sim::UnitType::Siege || t == dom::sim::UnitType::Bomber || t == dom::sim::UnitType::StrategicBomber) ++mix.rangedSupport;
  }
  return mix;
}


int phase_index(dom::sim::MatchFlowPhase phase) {
  switch (phase) {
    case dom::sim::MatchFlowPhase::EarlyExpansion: return 0;
    case dom::sim::MatchFlowPhase::RegionalContest: return 1;
    case dom::sim::MatchFlowPhase::IndustrialEscalation: return 2;
    case dom::sim::MatchFlowPhase::StrategicCrisis: return 3;
    case dom::sim::MatchFlowPhase::ArmageddonEndgame: return 4;
  }
  return 0;
}

void record_expansion(dom::sim::World& w, dom::sim::MatchFlowPhase phase) {
  const int idx = phase_index(phase);
  if (idx == 0) ++w.aiExpansionEarlyCount;
  else if (idx == 1) ++w.aiExpansionRegionalCount;
  else if (idx == 2) ++w.aiExpansionIndustrialCount;
  else if (idx == 3) ++w.aiExpansionCrisisCount;
  else ++w.aiExpansionArmageddonCount;
}

bool can_afford(const dom::sim::PlayerState& p, dom::sim::Resource r, float minAmount) {
  return p.resources[static_cast<size_t>(r)] >= minAmount;
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
  for (const auto& p : world.players) if (p.id != team && !dom::sim::players_allied(world, p.id, team) && p.alive) { enemyTeam = p.id; break; }
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
  const bool armageddon = world.armageddonActive;
  const auto flowPhase = dom::sim::compute_match_flow_phase(world);
  const auto& player = world.players[team];

  const float food = player.resources[static_cast<size_t>(dom::sim::Resource::Food)];
  const float metal = player.resources[static_cast<size_t>(dom::sim::Resource::Metal)];
  const float wealth = player.resources[static_cast<size_t>(dom::sim::Resource::Wealth)];
  const float knowledge = player.resources[static_cast<size_t>(dom::sim::Resource::Knowledge)];
  const float oil = player.resources[static_cast<size_t>(dom::sim::Resource::Oil)];
  const bool foodLow = food < 160.0f;
  const bool metalLow = metal < 130.0f;
  const bool wealthLow = wealth < 130.0f;
  const bool knowledgeLow = knowledge < 80.0f;
  const bool oilLow = oil < 55.0f && flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation;

  uint32_t cc = first_building(world, team, dom::sim::BuildingType::CityCenter);
  const int workerTargetBase = std::clamp((int)std::round((5.0f * civ.economyBias + 1.8f) * civ.aiWorkerTargetMult), 5, 18);
  const int workerTarget = workerTargetBase + (foodLow || metalLow ? 2 : 0);
  if (cc && count_units(world, team, dom::sim::UnitType::Worker) < workerTarget) dom::sim::enqueue_train_unit(world, team, cc, dom::sim::UnitType::Worker);

  auto maybeBuild = [&](dom::sim::BuildingType t, glm::vec2 offset) {
    if (count_buildings(world, team, t) == 0) {
      dom::sim::start_build_placement(world, team, t);
      dom::sim::update_build_placement(world, team, base + offset);
      if (dom::sim::confirm_build_placement(world, team)) record_expansion(world, flowPhase);
    }
  };

  auto maybeBuildRail = [&](const glm::vec2& p, dom::sim::RailNodeType type) {
    uint32_t nid = world.railNodes.empty() ? 1u : world.railNodes.back().id + 1;
    world.railNodes.push_back({nid, team, type, {(int)std::round(p.x), (int)std::round(p.y)}, 0u, true});
    ++world.aiRailUsageEvents;
    for (auto it = world.railNodes.rbegin() + 1; it != world.railNodes.rend(); ++it) {
      if (it->owner != team) continue;
      uint32_t eid = world.railEdges.empty() ? 1u : world.railEdges.back().id + 1;
      world.railEdges.push_back({eid, team, it->id, nid, 1, false, false, false});
      break;
    }
  };

  if (player.popCap - player.popUsed <= 3 || (flowPhase == dom::sim::MatchFlowPhase::EarlyExpansion && player.popCap < 18)) maybeBuild(dom::sim::BuildingType::House, {10, 5});
  glm::vec2 mountainMinePos{};
  const bool hasMountainSpot = find_mountain_mine_spot(world, team, mountainMinePos);
  if (count_buildings(world, team, dom::sim::BuildingType::Mine) == 0 || (metalLow && count_buildings(world, team, dom::sim::BuildingType::Mine) < 2 && flowPhase >= dom::sim::MatchFlowPhase::RegionalContest)) {
    dom::sim::start_build_placement(world, team, dom::sim::BuildingType::Mine);
    dom::sim::update_build_placement(world, team, hasMountainSpot ? mountainMinePos : base + glm::vec2{8, 8});
    if (dom::sim::confirm_build_placement(world, team)) record_expansion(world, flowPhase);
  }

  if (foodLow || civ.economyBias >= civ.militaryBias) maybeBuild(dom::sim::BuildingType::Farm, {6, 4});
  maybeBuild(dom::sim::BuildingType::LumberCamp, {4, 8});
  if (wealthLow || flowPhase >= dom::sim::MatchFlowPhase::RegionalContest || civ.economyBias > 1.08f) maybeBuild(dom::sim::BuildingType::Market, {11, 9});
  if ((knowledgeLow && flowPhase >= dom::sim::MatchFlowPhase::RegionalContest) || civ.scienceBias * civ.aiResearchPriority > 0.95f) maybeBuild(dom::sim::BuildingType::Library, {12, 12});

  const bool industrialEra = player.age >= dom::sim::Age::Industrial;
  const bool industrialWindow = industrialEra || flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation || (world.tick > 760 && civ.economyBias > 1.02f);
  if (industrialWindow) {
    const uint32_t priorIndustrial = count_buildings(world, team, dom::sim::BuildingType::SteelMill)
      + count_buildings(world, team, dom::sim::BuildingType::Refinery)
      + count_buildings(world, team, dom::sim::BuildingType::FactoryHub);
    if (count_buildings(world, team, dom::sim::BuildingType::SteelMill) == 0 && count_buildings(world, team, dom::sim::BuildingType::Mine) > 0 && can_afford(player, dom::sim::Resource::Metal, 80.0f)) maybeBuild(dom::sim::BuildingType::SteelMill, {12, 6});
    if (count_buildings(world, team, dom::sim::BuildingType::Refinery) == 0 && (can_afford(player, dom::sim::Resource::Metal, 70.0f) || oilLow)) maybeBuild(dom::sim::BuildingType::Refinery, {14, 5});
    if (count_buildings(world, team, dom::sim::BuildingType::MunitionsPlant) == 0 && civ.militaryBias >= 1.0f && !wealthLow) maybeBuild(dom::sim::BuildingType::MunitionsPlant, {15, 8});
    if (count_buildings(world, team, dom::sim::BuildingType::MachineWorks) == 0 && (civ.economyBias >= 1.0f || civ.militaryBias >= 1.1f)) maybeBuild(dom::sim::BuildingType::MachineWorks, {13, 10});
    if (count_buildings(world, team, dom::sim::BuildingType::ElectronicsLab) == 0 && civ.scienceBias >= 1.0f && !knowledgeLow) maybeBuild(dom::sim::BuildingType::ElectronicsLab, {10, 11});
    if (count_buildings(world, team, dom::sim::BuildingType::FactoryHub) == 0 && (civ.logisticsBias >= 1.0f || civ.economyBias >= 1.1f)) maybeBuild(dom::sim::BuildingType::FactoryHub, {16, 10});
    const uint32_t nowIndustrial = count_buildings(world, team, dom::sim::BuildingType::SteelMill)
      + count_buildings(world, team, dom::sim::BuildingType::Refinery)
      + count_buildings(world, team, dom::sim::BuildingType::FactoryHub);
    if (priorIndustrial == 0 && nowIndustrial > 0) {
      ++world.aiIndustrialActivationCount;
      if (world.aiIndustrialActivationTick == 0 || world.tick < world.aiIndustrialActivationTick) world.aiIndustrialActivationTick = world.tick;
    }
  }

  if ((industrialEra || flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation) && world.tick % 60 == 0) {
    const bool economyRail = civ.economyBias >= 1.1f || civ.logisticsBias >= 1.1f;
    const bool militaryRail = civ.militaryBias >= 1.1f || civ.aggression >= 1.05f;
    if (economyRail) {
      for (const auto& b : world.buildings) {
        if (b.team != team || (b.type != dom::sim::BuildingType::Mine && b.type != dom::sim::BuildingType::Market && b.type != dom::sim::BuildingType::FactoryHub)) continue;
        maybeBuildRail(b.pos, b.type == dom::sim::BuildingType::Mine ? dom::sim::RailNodeType::Depot : dom::sim::RailNodeType::Junction);
        break;
      }
    }
    if (militaryRail) maybeBuildRail(rally, dom::sim::RailNodeType::Station);
    if (civ.aggression >= 1.1f) {
      for (auto& e : world.railEdges) if (e.owner != team && !dom::sim::players_allied(world, e.owner, team)) { e.disrupted = true; ++world.aiRailUsageEvents; break; }
    }
    for (auto& e : world.railEdges) if (e.owner == team && e.disrupted) { e.disrupted = false; ++world.aiRailUsageEvents; break; }
  }

  maybeBuild(dom::sim::BuildingType::Barracks, {14, 8});
  if (civ.scienceBias * civ.aiReconPriority >= 1.0f || civ.defense >= 1.0f) maybeBuild(dom::sim::BuildingType::RadarTower, {9, 10});
  if (civ.militaryBias * civ.aiAirPriority >= 1.0f) maybeBuild(dom::sim::BuildingType::Airbase, {16, 10});
  if (flowPhase >= dom::sim::MatchFlowPhase::StrategicCrisis || world.worldTension > 48.0f || civ.scienceBias * civ.aiStrategicPriority > 1.1f || civ.strategicBias > 1.1f) maybeBuild(dom::sim::BuildingType::MissileSilo, {18, 12});
  if (world.worldTension > 30.0f || civ.defense > 1.0f) { maybeBuild(dom::sim::BuildingType::AABattery, {7, 13}); maybeBuild(dom::sim::BuildingType::AntiMissileDefense, {6, 15}); }

  const bool navalRelevant = map_has_navigable_water(world) && coastal_city_exists(world, team);
  if (navalRelevant) maybeBuild(dom::sim::BuildingType::Port, {9, 3});

  const ArmyMix enemyMix = scan_enemy_mix(world, enemyTeam);
  if (enemyMix.armor + enemyMix.air + enemyMix.staticBreak + enemyMix.rangedSupport > 0) ++world.aiCounterResponseEvents;

  uint32_t barracks = first_building(world, team, dom::sim::BuildingType::Barracks);
  if (barracks) {
    float infBias = civ.militaryBias;
    float archBias = civ.scienceBias;
    float cavBias = civ.aggression;
    float siegeBias = civ.defense;
    if (enemyMix.armor >= 2) { cavBias *= 1.2f; infBias *= 1.08f; }
    if (enemyMix.air >= 2) { archBias *= 1.2f; infBias *= 1.08f; }
    if (enemyMix.staticBreak >= 2) { siegeBias *= 1.2f; }
    if (enemyMix.rangedSupport >= 2) { cavBias *= 1.18f; }

    if (civ.id == "rome") { infBias *= 1.28f; siegeBias *= 1.12f; }
    else if (civ.id == "china") { archBias *= 1.2f; infBias *= 1.08f; }
    else if (civ.id == "usa") { archBias *= 1.1f; cavBias *= 1.08f; }
    else if (civ.id == "russia") { infBias *= 1.08f; siegeBias *= 1.3f; }
    else if (civ.id == "japan") { cavBias *= 1.15f; archBias *= 1.12f; }
    else if (civ.id == "egypt") { cavBias *= 1.16f; siegeBias *= 1.16f; }
    else if (civ.id == "tartaria") { cavBias *= 1.22f; siegeBias *= 1.18f; }

    const float earlyScale = flowPhase == dom::sim::MatchFlowPhase::EarlyExpansion ? 1.2f : 1.0f;
    const float regionalScale = flowPhase >= dom::sim::MatchFlowPhase::RegionalContest ? 1.0f : 0.35f;
    const float industrialScale = flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation ? 1.0f : 0.55f;

    const int infTarget = std::clamp((int)std::round(5.0f * infBias * earlyScale), 4, 20);
    const int archTarget = std::clamp((int)std::round((2.0f * archBias + 1.0f) * regionalScale), 1, 10);
    const int cavTarget = std::clamp((int)std::round((2.0f * cavBias + 1.0f) * regionalScale), 1, 10);
    const int siegeTarget = std::clamp((int)std::round((2.2f * siegeBias) * industrialScale), 1, 8);
    if (count_units(world, team, dom::sim::UnitType::Infantry) < infTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Infantry);
    if (count_units(world, team, dom::sim::UnitType::Archer) < archTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Archer);
    if (count_units(world, team, dom::sim::UnitType::Cavalry) < cavTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Cavalry);
    if (count_units(world, team, dom::sim::UnitType::Siege) < siegeTarget) dom::sim::enqueue_train_unit(world, team, barracks, dom::sim::UnitType::Siege);
  }

  uint32_t airbase = first_building(world, team, dom::sim::BuildingType::Airbase);
  if (airbase) {
    const float crisisBoost = flowPhase >= dom::sim::MatchFlowPhase::StrategicCrisis ? 1.3f : 1.0f;
    int fighterTarget = std::clamp((int)std::round((2.0f * civ.militaryBias + (world.worldTension > 40.0f ? 2.0f : 0.0f) + (enemyMix.air >= 2 ? 2.0f : 0.0f)) * crisisBoost), 1, 10);
    int interceptorTarget = std::clamp((int)std::round(1.0f * civ.defense + (flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation ? 1.0f : 0.0f) + (enemyMix.air >= 3 ? 2.0f : 0.0f)), 1, 7);
    int bomberTarget = std::clamp((int)std::round((1.6f * civ.militaryBias + (enemyMix.staticBreak >= 2 ? 1.0f : 0.0f)) * (flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation ? 1.0f : 0.4f)), 1, 7);
    int reconTarget = std::clamp((int)std::round(1.0f + 2.0f * civ.scienceBias), 1, 4);
    if (count_units(world, team, dom::sim::UnitType::Fighter) < fighterTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Fighter);
    if (count_units(world, team, dom::sim::UnitType::Interceptor) < interceptorTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Interceptor);
    if (count_units(world, team, dom::sim::UnitType::Bomber) < bomberTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::Bomber);
    if (count_units(world, team, dom::sim::UnitType::ReconDrone) < reconTarget) dom::sim::enqueue_train_unit(world, team, airbase, dom::sim::UnitType::ReconDrone);
  }

  uint32_t silo = first_building(world, team, dom::sim::BuildingType::MissileSilo);
  if (silo && (flowPhase >= dom::sim::MatchFlowPhase::StrategicCrisis || world.worldTension > 62.0f) && civ.scienceBias >= 1.0f) {
    if (count_units(world, team, dom::sim::UnitType::TacticalMissile) < 2) dom::sim::enqueue_train_unit(world, team, silo, dom::sim::UnitType::TacticalMissile);
    if (count_units(world, team, dom::sim::UnitType::StrategicMissile) < 1) dom::sim::enqueue_train_unit(world, team, silo, dom::sim::UnitType::StrategicMissile);
  }

  uint32_t lib = first_building(world, team, dom::sim::BuildingType::Library);
  if (lib && player.age < dom::sim::Age::Classical && civ.scienceBias >= 0.9f && !knowledgeLow) dom::sim::enqueue_age_research(world, team, lib);

  uint32_t port = first_building(world, team, dom::sim::BuildingType::Port);
  if (port && navalRelevant) {
    int tTarget = std::max(1, (int)std::round(civ.economyBias + (civ.id == "usa" ? 1.0f : 0.0f)));
    int lTarget = std::max(1, (int)std::round(civ.militaryBias + (civ.id == "japan" ? 1.0f : 0.0f)));
    int hTarget = std::max(1, (int)std::round(civ.aggression * (flowPhase >= dom::sim::MatchFlowPhase::RegionalContest ? 1.0f : 0.5f) + (civ.id == "japan" ? 1.0f : 0.0f)));
    int bTarget = std::max(1, (int)std::round(civ.defense * (flowPhase >= dom::sim::MatchFlowPhase::IndustrialEscalation ? 1.0f : 0.45f)));
    if (count_units(world, team, dom::sim::UnitType::TransportShip) < tTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::TransportShip);
    if (count_units(world, team, dom::sim::UnitType::LightWarship) < lTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::LightWarship);
    if (count_units(world, team, dom::sim::UnitType::HeavyWarship) < hTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::HeavyWarship);
    if (count_units(world, team, dom::sim::UnitType::BombardShip) < bTarget) dom::sim::enqueue_train_unit(world, team, port, dom::sim::UnitType::BombardShip);
  }

  if (flowPhase >= dom::sim::MatchFlowPhase::StrategicCrisis && world.tick % 100 == 0) ++world.aiDeterrencePostureChanges;
  if (world.tick % 120 == 0) {
    for (const auto& op : world.operations) {
      if (op.team == team && op.active) { ++world.aiOperationLaunches; break; }
    }
  }

  std::vector<uint32_t> army;
  for (const auto& u : world.units) if (u.team == team && u.type != dom::sim::UnitType::Worker) army.push_back(u.id);
  std::sort(army.begin(), army.end());
  if (army.empty()) return;

  const int combatArmy = static_cast<int>(army.size());
  for (const auto& s : world.guardianSites) {
    const float d = glm::length(s.pos - base);
    const bool nearBase = d < 50.0f;
    const bool hostileType = s.guardianId == "kraken" || s.guardianId == "sandworm";
    const bool beneficialType = s.guardianId == "snow_yeti" || s.guardianId == "forest_spirit";
    if (!s.discovered && s.siteActive && !s.siteDepleted && nearBase && beneficialType && combatArmy >= 4) {
      dom::sim::issue_move(world, team, army, s.pos);
      ++world.aiDecisionCount;
      return;
    }
    if (s.discovered && s.spawned && hostileType && nearBase && combatArmy < 6) {
      dom::sim::issue_move(world, team, army, rally);
      ++world.aiRetreatCount;
      ++world.aiDecisionCount;
      return;
    }
  }

  float tensionFactor = std::clamp(1.0f - world.worldTension / 180.0f, 0.5f, 1.2f);
  const float expansionTiming = std::max(0.7f, civ.aiExpansionTiming);
  int attackThreshold = std::clamp((int)std::round((((gAttackEarly ? 5.0f : 8.0f) * expansionTiming) / std::max(0.7f, civ.aggression)) * tensionFactor), 3, 12);
  if (armageddon) {
    const float civStyle = std::max(0.85f, civ.aggression * civ.militaryBias);
    attackThreshold = std::clamp((int)std::round(attackThreshold / civStyle), 2, 10);
  }
  if ((int)army.size() < attackThreshold) { ++world.aiDecisionCount; return; }

  TeamStrength ours = strength_near(world, team, enemyBase, 30.0f);
  TeamStrength theirs = strength_near(world, enemyTeam, enemyBase, 30.0f);

  dom::sim::OperationType opType = dom::sim::OperationType::RallyAndPush;
  glm::vec2 opTarget = enemyBase;
  for (const auto& op : world.operations) {
    if (op.team == team && op.active) { opType = op.type; opTarget = op.target; break; }
  }
  if (opType == dom::sim::OperationType::DefendBorder || opType == dom::sim::OperationType::SecureRoute) opTarget = rally;
  if (flowPhase == dom::sim::MatchFlowPhase::EarlyExpansion) opTarget = rally;
  if (armageddon) {
    if (civ.id == "russia") opTarget = rally;
    else if (civ.id == "usa" || civ.id == "uk") opTarget = enemyBase;
    else if (civ.id == "japan") opTarget = navalRelevant ? enemyBase : rally;
    else if (civ.id == "eu") opTarget = (theirs.hp > ours.hp) ? rally : enemyBase;
    else if (civ.id == "egypt") opTarget = rally;
    else if (civ.id == "tartaria") opTarget = enemyBase;
  }

  int outSupply = 0;
  for (const auto& u : world.units) if (u.team == team && u.type != dom::sim::UnitType::Worker && u.supplyState == dom::sim::SupplyState::OutOfSupply) ++outSupply;
  if (outSupply > std::max(2, (int)army.size() / 4)) ++world.aiLogisticsDisruptedFronts;

  int retreatHpThreshold = (int)std::round((gAggressive ? 240.0f : 360.0f) * std::max(0.75f, civ.defense));
  if (armageddon) {
    if (civ.id == "russia" || civ.id == "egypt") retreatHpThreshold = (int)std::round(retreatHpThreshold * 1.15f);
    if (civ.id == "usa" || civ.id == "tartaria") retreatHpThreshold = (int)std::round(retreatHpThreshold * 0.9f);
  }
  const bool shouldRetreat = ours.hp < retreatHpThreshold || (theirs.hp > 0 && ours.hp * 100 < theirs.hp * (gAggressive ? 60 : 80)) || outSupply > (int)army.size() / 3;

  if (shouldRetreat) {
    dom::sim::issue_move(world, team, army, rally);
    ++world.aiRetreatCount;
  } else {
    if (opType == dom::sim::OperationType::RaidEconomy || flowPhase == dom::sim::MatchFlowPhase::StrategicCrisis) dom::sim::issue_move(world, team, army, opTarget);
    else dom::sim::issue_attack_move(world, team, army, opTarget);
  }

  ++world.aiDecisionCount;
}
}
