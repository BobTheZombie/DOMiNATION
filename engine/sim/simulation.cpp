#include "engine/sim/simulation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <queue>
#include <sstream>
#include <tuple>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <nlohmann/json.hpp>

namespace dom::sim {
namespace {
float dist(glm::vec2 a, glm::vec2 b) { return glm::length(a - b); }

constexpr uint64_t kFNVOffset = 1469598103934665603ull;
constexpr uint64_t kFNVPrime = 1099511628211ull;

struct BuildDef {
  glm::vec2 size{2.0f, 2.0f};
  float buildTime{10.0f};
  std::array<float, static_cast<size_t>(Resource::Count)> cost{};
  std::array<float, static_cast<size_t>(Resource::Count)> trickle{};
  int popCapBonus{0};
};

struct UnitDef {
  float trainTime{10.0f};
  int popCost{1};
  std::array<float, static_cast<size_t>(Resource::Count)> cost{};
  UnitRole role{UnitRole::Infantry};
  AttackType attackType{AttackType::Melee};
  UnitRole preferredTargetRole{UnitRole::Infantry};
  std::array<uint16_t, 6> vsRoleMultiplierPermille{1000, 1000, 1000, 1000, 1000, 1000};
  int attackCooldownTicks{12};
  int buildingHp{1000};
};

BuildDef gBuildDefs[9];
UnitDef gUnitDefs[5];
float gAgeResearchTime{30.0f};
std::array<float, static_cast<size_t>(Resource::Count)> gAgeResearchCost{};
bool gDefsLoaded{false};
bool gNavDebug{false};
bool gCombatDebug{false};
std::vector<ReplayCommand> gReplayCommands;

constexpr uint16_t kRoleCount = 6;
constexpr int kTargetBetterThreshold = 220;
constexpr int kAttackMoveAggroPermille = 8500;
constexpr int kAttackMoveChasePermille = 12000;
constexpr int kTargetLockMinTicks = 18;
constexpr int kCentroidLeashPermille = 18000;
constexpr int kMaxOrderPathLingerTicks = 90;

int role_idx(UnitRole role) { return static_cast<int>(role); }

struct FlowField {
  int targetCell{-1};
  uint32_t navVersion{0};
  int width{0};
  int height{0};
  std::vector<int32_t> integration;
  std::vector<int8_t> dirX;
  std::vector<int8_t> dirY;
};

struct NavRuntime {
  std::vector<FlowField> cache;
  std::vector<int32_t> workIntegration;
  std::vector<uint8_t> blocked;
  uint32_t nextMoveOrder{1};
} gNav;

constexpr int32_t kInfCost = 1 << 29;
constexpr std::array<std::pair<int,int>, 8> kNeighborOrder{{{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}}};

size_t ridx(Resource r) { return static_cast<size_t>(r); }
int bidx(BuildingType t) { return static_cast<int>(t); }
int uidx(UnitType t) { return static_cast<int>(t); }
int ridx_node(ResourceNodeType t) { return static_cast<int>(t); }

const char* building_name(BuildingType t) {
  switch (t) {
    case BuildingType::CityCenter: return "CityCenter";
    case BuildingType::House: return "House";
    case BuildingType::Farm: return "Farm";
    case BuildingType::LumberCamp: return "LumberCamp";
    case BuildingType::Mine: return "Mine";
    case BuildingType::Market: return "Market";
    case BuildingType::Library: return "Library";
    case BuildingType::Barracks: return "Barracks";
    case BuildingType::Wonder: return "Wonder";
  }
  return "House";
}

BuildingType parse_building(const std::string& v) {
  if (v == "CityCenter") return BuildingType::CityCenter;
  if (v == "Farm") return BuildingType::Farm;
  if (v == "LumberCamp") return BuildingType::LumberCamp;
  if (v == "Mine") return BuildingType::Mine;
  if (v == "Market") return BuildingType::Market;
  if (v == "Library") return BuildingType::Library;
  if (v == "Barracks") return BuildingType::Barracks;
  if (v == "Wonder") return BuildingType::Wonder;
  return BuildingType::House;
}

const char* unit_name(UnitType t) {
  switch (t) {
    case UnitType::Worker: return "Worker";
    case UnitType::Infantry: return "Infantry";
    case UnitType::Archer: return "Archer";
    case UnitType::Cavalry: return "Cavalry";
    case UnitType::Siege: return "Siege";
  }
  return "Infantry";
}

UnitType parse_unit(const std::string& v) {
  if (v == "Worker") return UnitType::Worker;
  if (v == "Archer") return UnitType::Archer;
  if (v == "Cavalry") return UnitType::Cavalry;
  if (v == "Siege") return UnitType::Siege;
  return UnitType::Infantry;
}

ObjectiveState parse_objective_state(const std::string& v) {
  if (v == "active") return ObjectiveState::Active;
  if (v == "completed") return ObjectiveState::Completed;
  if (v == "failed") return ObjectiveState::Failed;
  return ObjectiveState::Inactive;
}

const char* objective_state_name(ObjectiveState s) {
  switch (s) {
    case ObjectiveState::Inactive: return "inactive";
    case ObjectiveState::Active: return "active";
    case ObjectiveState::Completed: return "completed";
    case ObjectiveState::Failed: return "failed";
  }
  return "inactive";
}

struct CivilizationDef {
  std::string id{"default"};
  std::string displayName{"Default"};
  float economyBias{1.0f};
  float militaryBias{1.0f};
  float scienceBias{1.0f};
  float aggression{1.0f};
  float defense{1.0f};
};

std::vector<CivilizationDef> gCivilizations;

void load_civilizations_once() {
  if (!gCivilizations.empty()) return;
  gCivilizations.push_back({});
  std::ifstream f("content/civilizations.json");
  if (!f.good()) return;
  nlohmann::json j; f >> j;
  if (!j.is_array()) return;
  gCivilizations.clear();
  for (const auto& c : j) {
    CivilizationDef d{};
    d.id = c.value("id", std::string("default"));
    d.displayName = c.value("displayName", d.id);
    d.economyBias = c.value("economyBias", 1.0f);
    d.militaryBias = c.value("militaryBias", 1.0f);
    d.scienceBias = c.value("scienceBias", 1.0f);
    d.aggression = c.value("aggression", 1.0f);
    d.defense = c.value("defense", 1.0f);
    gCivilizations.push_back(d);
  }
  if (gCivilizations.empty()) gCivilizations.push_back({});
}

CivilizationRuntime civilization_runtime_for(const std::string& id) {
  load_civilizations_once();
  for (const auto& c : gCivilizations) if (c.id == id) return {c.id, c.economyBias, c.militaryBias, c.scienceBias, c.aggression, c.defense};
  return {"default", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
}


void hash_u32(uint64_t& h, uint32_t v) { h ^= static_cast<uint64_t>(v); h *= kFNVPrime; }
void hash_float(uint64_t& h, float v) { hash_u32(h, static_cast<uint32_t>(v * 1000.0f)); }

void set_default_defs() {
  gBuildDefs[bidx(BuildingType::CityCenter)].size = {3.6f, 3.6f};
  gBuildDefs[bidx(BuildingType::CityCenter)].buildTime = 0.0f;
  gBuildDefs[bidx(BuildingType::CityCenter)].popCapBonus = 10;

  gBuildDefs[bidx(BuildingType::House)].size = {2.2f, 2.2f};
  gBuildDefs[bidx(BuildingType::House)].buildTime = 12.0f;
  gBuildDefs[bidx(BuildingType::House)].cost[ridx(Resource::Wood)] = 70;
  gBuildDefs[bidx(BuildingType::House)].popCapBonus = 5;

  gBuildDefs[bidx(BuildingType::Farm)].size = {2.6f, 2.6f};
  gBuildDefs[bidx(BuildingType::Farm)].buildTime = 14.0f;
  gBuildDefs[bidx(BuildingType::Farm)].cost[ridx(Resource::Wood)] = 80;
  gBuildDefs[bidx(BuildingType::Farm)].trickle[ridx(Resource::Food)] = 1.9f;

  gBuildDefs[bidx(BuildingType::LumberCamp)].size = {2.6f, 2.6f};
  gBuildDefs[bidx(BuildingType::LumberCamp)].buildTime = 14.0f;
  gBuildDefs[bidx(BuildingType::LumberCamp)].cost[ridx(Resource::Wood)] = 75;
  gBuildDefs[bidx(BuildingType::LumberCamp)].trickle[ridx(Resource::Wood)] = 1.6f;

  gBuildDefs[bidx(BuildingType::Mine)].size = {2.6f, 2.6f};
  gBuildDefs[bidx(BuildingType::Mine)].buildTime = 15.0f;
  gBuildDefs[bidx(BuildingType::Mine)].cost[ridx(Resource::Wood)] = 60;
  gBuildDefs[bidx(BuildingType::Mine)].cost[ridx(Resource::Metal)] = 40;
  gBuildDefs[bidx(BuildingType::Mine)].trickle[ridx(Resource::Metal)] = 1.4f;

  gBuildDefs[bidx(BuildingType::Market)].size = {2.8f, 2.8f};
  gBuildDefs[bidx(BuildingType::Market)].buildTime = 18.0f;
  gBuildDefs[bidx(BuildingType::Market)].cost[ridx(Resource::Wood)] = 110;
  gBuildDefs[bidx(BuildingType::Market)].cost[ridx(Resource::Metal)] = 40;
  gBuildDefs[bidx(BuildingType::Market)].trickle[ridx(Resource::Wealth)] = 1.7f;

  gBuildDefs[bidx(BuildingType::Library)].size = {2.8f, 2.8f};
  gBuildDefs[bidx(BuildingType::Library)].buildTime = 18.0f;
  gBuildDefs[bidx(BuildingType::Library)].cost[ridx(Resource::Wood)] = 120;
  gBuildDefs[bidx(BuildingType::Library)].cost[ridx(Resource::Wealth)] = 60;
  gBuildDefs[bidx(BuildingType::Library)].trickle[ridx(Resource::Knowledge)] = 1.5f;

  gBuildDefs[bidx(BuildingType::Barracks)].size = {3.0f, 3.0f};
  gBuildDefs[bidx(BuildingType::Barracks)].buildTime = 20.0f;
  gBuildDefs[bidx(BuildingType::Barracks)].cost[ridx(Resource::Wood)] = 130;
  gBuildDefs[bidx(BuildingType::Barracks)].cost[ridx(Resource::Metal)] = 70;

  gBuildDefs[bidx(BuildingType::Wonder)].size = {4.0f, 4.0f};
  gBuildDefs[bidx(BuildingType::Wonder)].buildTime = 45.0f;
  gBuildDefs[bidx(BuildingType::Wonder)].cost[ridx(Resource::Wood)] = 350;
  gBuildDefs[bidx(BuildingType::Wonder)].cost[ridx(Resource::Metal)] = 300;
  gBuildDefs[bidx(BuildingType::Wonder)].cost[ridx(Resource::Wealth)] = 250;

  gUnitDefs[uidx(UnitType::Worker)].trainTime = 10.0f;
  gUnitDefs[uidx(UnitType::Worker)].cost[ridx(Resource::Food)] = 60;
  gUnitDefs[uidx(UnitType::Worker)].popCost = 1;
  gUnitDefs[uidx(UnitType::Worker)].role = UnitRole::Worker;
  gUnitDefs[uidx(UnitType::Worker)].attackType = AttackType::Melee;
  gUnitDefs[uidx(UnitType::Worker)].preferredTargetRole = UnitRole::Worker;

  gUnitDefs[uidx(UnitType::Infantry)].trainTime = 12.0f;
  gUnitDefs[uidx(UnitType::Infantry)].cost[ridx(Resource::Food)] = 70;
  gUnitDefs[uidx(UnitType::Infantry)].cost[ridx(Resource::Metal)] = 30;
  gUnitDefs[uidx(UnitType::Infantry)].popCost = 1;
  gUnitDefs[uidx(UnitType::Infantry)].role = UnitRole::Infantry;
  gUnitDefs[uidx(UnitType::Infantry)].attackType = AttackType::Melee;
  gUnitDefs[uidx(UnitType::Infantry)].preferredTargetRole = UnitRole::Ranged;
  gUnitDefs[uidx(UnitType::Infantry)].vsRoleMultiplierPermille = {1000, 1300, 900, 900, 1000, 1000};

  gUnitDefs[uidx(UnitType::Archer)].trainTime = 13.0f;
  gUnitDefs[uidx(UnitType::Archer)].cost[ridx(Resource::Food)] = 65;
  gUnitDefs[uidx(UnitType::Archer)].cost[ridx(Resource::Wood)] = 35;
  gUnitDefs[uidx(UnitType::Archer)].popCost = 1;
  gUnitDefs[uidx(UnitType::Archer)].role = UnitRole::Ranged;
  gUnitDefs[uidx(UnitType::Archer)].attackType = AttackType::Ranged;
  gUnitDefs[uidx(UnitType::Archer)].preferredTargetRole = UnitRole::Cavalry;
  gUnitDefs[uidx(UnitType::Archer)].vsRoleMultiplierPermille = {1000, 900, 1300, 1000, 1000, 900};
  gUnitDefs[uidx(UnitType::Archer)].attackCooldownTicks = 16;

  gUnitDefs[uidx(UnitType::Cavalry)].trainTime = 16.0f;
  gUnitDefs[uidx(UnitType::Cavalry)].cost[ridx(Resource::Food)] = 95;
  gUnitDefs[uidx(UnitType::Cavalry)].cost[ridx(Resource::Metal)] = 45;
  gUnitDefs[uidx(UnitType::Cavalry)].popCost = 2;
  gUnitDefs[uidx(UnitType::Cavalry)].role = UnitRole::Cavalry;
  gUnitDefs[uidx(UnitType::Cavalry)].attackType = AttackType::Melee;
  gUnitDefs[uidx(UnitType::Cavalry)].preferredTargetRole = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::Cavalry)].vsRoleMultiplierPermille = {1000, 1000, 900, 1300, 1100, 900};

  gUnitDefs[uidx(UnitType::Siege)].trainTime = 18.0f;
  gUnitDefs[uidx(UnitType::Siege)].cost[ridx(Resource::Wood)] = 90;
  gUnitDefs[uidx(UnitType::Siege)].cost[ridx(Resource::Metal)] = 100;
  gUnitDefs[uidx(UnitType::Siege)].popCost = 2;
  gUnitDefs[uidx(UnitType::Siege)].role = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::Siege)].attackType = AttackType::Ranged;
  gUnitDefs[uidx(UnitType::Siege)].preferredTargetRole = UnitRole::Building;
  gUnitDefs[uidx(UnitType::Siege)].vsRoleMultiplierPermille = {900, 900, 900, 1000, 900, 1800};
  gUnitDefs[uidx(UnitType::Siege)].attackCooldownTicks = 22;

  gAgeResearchTime = 35.0f;
  gAgeResearchCost[ridx(Resource::Knowledge)] = 130;
  gAgeResearchCost[ridx(Resource::Wealth)] = 110;
}

void load_defs_once() {
  if (gDefsLoaded) return;
  set_default_defs();
  std::ifstream f("game/content/default_content.json");
  if (!f.good()) { gDefsLoaded = true; return; }
  nlohmann::json j; f >> j;
  auto parseCost = [](const nlohmann::json& cj, std::array<float, static_cast<size_t>(Resource::Count)>& out) {
    auto setIf = [&](const char* k, Resource r) { if (cj.contains(k)) out[ridx(r)] = cj[k].get<float>(); };
    setIf("food", Resource::Food); setIf("wood", Resource::Wood); setIf("metal", Resource::Metal);
    setIf("wealth", Resource::Wealth); setIf("knowledge", Resource::Knowledge); setIf("oil", Resource::Oil);
  };
  if (j.contains("buildingDefs")) {
    for (const auto& bd : j["buildingDefs"]) {
      std::string id = bd.value("id", "");
      BuildingType t = BuildingType::House;
      if (id == "CityCenter") t = BuildingType::CityCenter;
      else if (id == "Farm") t = BuildingType::Farm;
      else if (id == "LumberCamp") t = BuildingType::LumberCamp;
      else if (id == "Mine") t = BuildingType::Mine;
      else if (id == "Market") t = BuildingType::Market;
      else if (id == "Library") t = BuildingType::Library;
      else if (id == "Barracks") t = BuildingType::Barracks;
      else if (id == "Wonder") t = BuildingType::Wonder;
      BuildDef& d = gBuildDefs[bidx(t)];
      if (bd.contains("size")) d.size = {bd["size"][0].get<float>(), bd["size"][1].get<float>()};
      d.buildTime = bd.value("buildTime", d.buildTime);
      d.popCapBonus = bd.value("popCapBonus", d.popCapBonus);
      if (bd.contains("cost")) parseCost(bd["cost"], d.cost);
      if (bd.contains("trickle")) parseCost(bd["trickle"], d.trickle);
    }
  }
  if (j.contains("unitDefs")) {
    for (const auto& ud : j["unitDefs"]) {
      std::string id = ud.value("id", "");
      UnitType t = UnitType::Infantry;
      if (id == "Worker") t = UnitType::Worker;
      else if (id == "Archer") t = UnitType::Archer;
      else if (id == "Cavalry") t = UnitType::Cavalry;
      else if (id == "Siege") t = UnitType::Siege;
      UnitDef& d = gUnitDefs[uidx(t)];
      d.trainTime = ud.value("trainTime", d.trainTime);
      d.popCost = ud.value("popCost", d.popCost);
      if (ud.contains("cost")) parseCost(ud["cost"], d.cost);
    }
  }
  if (j.contains("ageUp")) {
    gAgeResearchTime = j["ageUp"].value("researchTime", gAgeResearchTime);
    if (j["ageUp"].contains("cost")) parseCost(j["ageUp"]["cost"], gAgeResearchCost);
  }
  gDefsLoaded = true;
}

bool can_afford(const std::array<float, static_cast<size_t>(Resource::Count)>& pool, const std::array<float, static_cast<size_t>(Resource::Count)>& cost) {
  for (size_t i = 0; i < cost.size(); ++i) if (pool[i] + 0.001f < cost[i]) return false;
  return true;
}

bool spend(std::array<float, static_cast<size_t>(Resource::Count)>& pool, const std::array<float, static_cast<size_t>(Resource::Count)>& cost) {
  if (!can_afford(pool, cost)) return false;
  for (size_t i = 0; i < cost.size(); ++i) pool[i] -= cost[i];
  return true;
}

void refund(std::array<float, static_cast<size_t>(Resource::Count)>& pool, const std::array<float, static_cast<size_t>(Resource::Count)>& cost, float frac) {
  for (size_t i = 0; i < cost.size(); ++i) pool[i] += cost[i] * frac;
}

bool has_nearby_builder(const World& w, uint16_t team, glm::vec2 p) {
  for (const auto& u : w.units) if (u.team == team && u.type == UnitType::Worker && u.hp > 0 && dist(u.pos, p) < 6.0f) return true;
  return false;
}

float fertility_at(const World& w, glm::vec2 p) {
  int x = std::clamp((int)p.x, 0, w.width - 1);
  int y = std::clamp((int)p.y, 0, w.height - 1);
  return w.fertility[y * w.width + x];
}

int cell_of(const World& w, glm::vec2 p) {
  int x = std::clamp((int)p.x, 0, w.width - 1);
  int y = std::clamp((int)p.y, 0, w.height - 1);
  return y * w.width + x;
}

glm::vec2 cell_center(const World& w, int idx) {
  int x = idx % w.width;
  int y = idx / w.width;
  return glm::vec2{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
}

void build_blocked_grid(const World& w) {
  const int cells = w.width * w.height;
  if ((int)gNav.blocked.size() != cells) gNav.blocked.assign(cells, 0);
  else std::fill(gNav.blocked.begin(), gNav.blocked.end(), 0);
  for (const auto& b : w.buildings) {
    int minX = std::max(0, (int)std::floor(b.pos.x - b.size.x * 0.5f));
    int maxX = std::min(w.width - 1, (int)std::ceil(b.pos.x + b.size.x * 0.5f));
    int minY = std::max(0, (int)std::floor(b.pos.y - b.size.y * 0.5f));
    int maxY = std::min(w.height - 1, (int)std::ceil(b.pos.y + b.size.y * 0.5f));
    for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) gNav.blocked[y * w.width + x] = 1;
  }
}

int cell_step_cost(const World& w, int fromCell, int toCell) {
  const float hf = w.heightmap[fromCell];
  const float ht = w.heightmap[toCell];
  const int slope = (int)std::round(std::abs(ht - hf) * 30.0f);
  return 10 + slope;
}

FlowField* get_flow_field(World& w, int targetCell) {
  for (auto& f : gNav.cache) {
    if (f.targetCell == targetCell && f.navVersion == w.navVersion && f.width == w.width && f.height == w.height) {
      ++w.flowFieldCacheHitCount;
      return &f;
    }
  }
  FlowField f{};
  f.targetCell = targetCell;
  f.navVersion = w.navVersion;
  f.width = w.width;
  f.height = w.height;
  const int cells = w.width * w.height;
  f.integration.assign(cells, kInfCost);
  f.dirX.assign(cells, 0);
  f.dirY.assign(cells, 0);
  build_blocked_grid(w);
  std::queue<int> q;
  f.integration[targetCell] = 0;
  q.push(targetCell);
  while (!q.empty()) {
    const int cur = q.front(); q.pop();
    const int cx = cur % w.width;
    const int cy = cur / w.width;
    const int32_t base = f.integration[cur];
    for (const auto& n : kNeighborOrder) {
      int nx = cx + n.first;
      int ny = cy + n.second;
      if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
      const int ni = ny * w.width + nx;
      if (gNav.blocked[ni]) continue;
      int32_t cand = base + cell_step_cost(w, cur, ni);
      if (cand < f.integration[ni]) { f.integration[ni] = cand; q.push(ni); }
    }
  }
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      const int idx = y * w.width + x;
      int32_t best = f.integration[idx];
      int8_t bx = 0, by = 0;
      for (const auto& n : kNeighborOrder) {
        int nx = x + n.first;
        int ny = y + n.second;
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        const int ni = ny * w.width + nx;
        if (f.integration[ni] < best) { best = f.integration[ni]; bx = (int8_t)n.first; by = (int8_t)n.second; }
      }
      f.dirX[idx] = bx;
      f.dirY[idx] = by;
    }
  }
  ++w.flowFieldGeneratedCount;
  gNav.cache.push_back(std::move(f));
  if (gNav.cache.size() > 32) gNav.cache.erase(gNav.cache.begin());
  return &gNav.cache.back();
}

void recompute_population(World& w) {
  for (auto& p : w.players) { p.popUsed = 0; p.popCap = 0; }
  for (const auto& b : w.buildings) {
    if (b.underConstruction) continue;
    w.players[b.team].popCap += gBuildDefs[bidx(b.type)].popCapBonus;
  }
  for (const auto& u : w.units) if (u.hp > 0) w.players[u.team].popUsed += gUnitDefs[uidx(u.type)].popCost;
}

bool placeable(const World& w, uint16_t team, BuildingType type, glm::vec2 pos) {
  const BuildDef& d = gBuildDefs[bidx(type)];
  if (pos.x < d.size.x || pos.y < d.size.y || pos.x > w.width - d.size.x || pos.y > w.height - d.size.y) return false;
  int tx = std::clamp((int)pos.x, 0, w.width - 1), ty = std::clamp((int)pos.y, 0, w.height - 1);
  bool territoryOk = w.territoryOwner[ty * w.width + tx] == team;
  if (!territoryOk) {
    for (const auto& c : w.cities) if (c.team == team && dist(c.pos, pos) <= 16.0f) territoryOk = true;
  }
  if (!territoryOk) return false;
  float centerH = w.heightmap[ty * w.width + tx];
  for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
    int nx = std::clamp(tx + ox, 0, w.width - 1), ny = std::clamp(ty + oy, 0, w.height - 1);
    if (std::abs(w.heightmap[ny * w.width + nx] - centerH) > 0.25f) return false;
  }
  for (const auto& b : w.buildings) {
    float sx = (d.size.x + b.size.x) * 0.55f;
    float sy = (d.size.y + b.size.y) * 0.55f;
    if (std::abs(b.pos.x - pos.x) < sx && std::abs(b.pos.y - pos.y) < sy) return false;
  }
  return true;
}

void recompute_territory(World& w) {
  std::fill(w.territoryOwner.begin(), w.territoryOwner.end(), 0);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      float best = 1e9f;
      uint16_t owner = 0;
      glm::vec2 p{x + 0.5f, y + 0.5f};
      for (const auto& c : w.cities) {
        float d = dist(c.pos, p) / (c.capital ? 1.4f : 1.0f);
        if (d < best && d < 22.0f + c.level * 2.0f) { best = d; owner = c.team; }
      }
      w.territoryOwner[y * w.width + x] = owner;
    }
  }
  ++w.territoryRecomputeCount;
  w.territoryDirty = true;
}

void recompute_fog(World& w) {
  std::fill(w.fog.begin(), w.fog.end(), w.godMode ? 255 : 0);
  if (w.godMode) { w.fogDirty = true; return; }
  for (const auto& c : w.cities) if (c.team == 0) {
    for (int y = std::max(0, static_cast<int>(c.pos.y) - 10); y <= std::min(w.height - 1, static_cast<int>(c.pos.y) + 10); ++y)
      for (int x = std::max(0, static_cast<int>(c.pos.x) - 10); x <= std::min(w.width - 1, static_cast<int>(c.pos.x) + 10); ++x)
        if (dist({x + 0.5f, y + 0.5f}, c.pos) < 10.5f) w.fog[y * w.width + x] = 255;
  }
  for (const auto& u : w.units) if (u.team == 0 && u.hp > 0) {
    for (int y = std::max(0, static_cast<int>(u.pos.y) - 7); y <= std::min(w.height - 1, static_cast<int>(u.pos.y) + 7); ++y)
      for (int x = std::max(0, static_cast<int>(u.pos.x) - 7); x <= std::min(w.width - 1, static_cast<int>(u.pos.x) + 7); ++x)
        if (dist({x + 0.5f, y + 0.5f}, u.pos) < 7.5f) w.fog[y * w.width + x] = 255;
  }
  w.fogDirty = true;
}



uint32_t find_enemy_near(const World& w, const Unit& u, float radius) {
  uint32_t bestId = 0;
  int bestScore = -1;
  for (const auto& e : w.units) {
    if (players_allied(w, e.team, u.team) || e.hp <= 0) continue;
    float d = dist(u.pos, e.pos);
    if (d > radius) continue;
    int score = 5000 - static_cast<int>(d * 800.0f);
    if (e.hp < 40.0f) score += 250;
    if (e.role == u.preferredTargetRole) score += 400;
    score += static_cast<int>(u.vsRoleMultiplierPermille[role_idx(e.role)]) - 1000;
    if (u.role == UnitRole::Siege && e.role == UnitRole::Building) score += 600;
    if (score > bestScore || (score == bestScore && e.id < bestId)) { bestScore = score; bestId = e.id; }
  }
  return bestId;
}

int find_building_target(const World& w, const Unit& u, float radius) {
  int best = -1;
  int bestScore = -1;
  for (int i = 0; i < static_cast<int>(w.buildings.size()); ++i) {
    const auto& b = w.buildings[i];
    if (players_allied(w, b.team, u.team) || b.underConstruction || b.hp <= 0.0f) continue;
    float d = dist(u.pos, b.pos);
    if (d > radius) continue;
    int score = 4800 - static_cast<int>(d * 800.0f);
    if (u.role == UnitRole::Siege) score += 900;
    if (score > bestScore || (score == bestScore && b.id < w.buildings[best].id)) { bestScore = score; best = i; }
  }
  return best;
}

Unit* find_unit(World& w, uint32_t id) {
  for (auto& u : w.units) if (u.id == id && u.hp > 0) return &u;
  return nullptr;
}

glm::vec2 group_centroid_for_order(const World& w, const Unit& member) {
  if (member.moveOrder == 0) return member.pos;
  glm::vec2 c{0.0f, 0.0f};
  int n = 0;
  for (const auto& u : w.units) {
    if (u.team != member.team || u.hp <= 0 || u.moveOrder != member.moveOrder) continue;
    c += u.pos;
    ++n;
  }
  return n > 0 ? c / static_cast<float>(n) : member.pos;
}

uint32_t spawn_unit(World& w, uint16_t team, UnitType type, glm::vec2 p) {
  uint32_t id = 1;
  for (const auto& u : w.units) id = std::max(id, u.id + 1);
  Unit nu{}; nu.id = id; nu.team = team; nu.type = type; nu.pos = nu.renderPos = nu.target = nu.slotTarget = p;
  const UnitDef& ud = gUnitDefs[uidx(type)];
  nu.role = ud.role;
  nu.attackType = ud.attackType;
  nu.preferredTargetRole = ud.preferredTargetRole;
  nu.vsRoleMultiplierPermille = ud.vsRoleMultiplierPermille;
  nu.attackCooldownTicks = 0;
  if (type == UnitType::Worker) { nu.hp = 70; nu.attack = 3.0f; nu.range = 1.5f; nu.speed = 4.3f; }
  else if (type == UnitType::Infantry) { nu.hp = 105; nu.attack = 8.5f; nu.range = 2.0f; nu.speed = 4.8f; }
  else if (type == UnitType::Archer) { nu.hp = 80; nu.attack = 7.0f; nu.range = 5.4f; nu.speed = 4.4f; }
  else if (type == UnitType::Cavalry) { nu.hp = 130; nu.attack = 9.2f; nu.range = 1.8f; nu.speed = 5.6f; }
  else { nu.hp = 110; nu.attack = 13.0f; nu.range = 6.2f; nu.speed = 3.2f; }
  w.units.push_back(nu);
  return id;
}


void apply_match_end(World& w, VictoryCondition condition, uint16_t winner, bool scoreTieBreak) {
  if (w.match.phase != MatchPhase::Running) return;
  w.match.phase = MatchPhase::Ended;
  w.match.condition = condition;
  w.match.winner = winner;
  w.match.endTick = w.tick;
  w.match.scoreTieBreak = scoreTieBreak;
  w.gameOver = true;
  w.winner = winner;
}

int controlled_capitals(const World& w, uint16_t team) {
  int caps = 0;
  for (const auto& c : w.cities) {
    if (!c.capital || c.team != team) continue;
    bool alive = false;
    for (const auto& b : w.buildings) {
      if (b.team == team && b.type == BuildingType::CityCenter && !b.underConstruction && b.hp > 0.0f && dist(b.pos, c.pos) < 8.0f) { alive = true; break; }
    }
    if (alive) ++caps;
  }
  return caps;
}

} // namespace

bool gameplay_orders_allowed(const World& world) { return world.match.phase == MatchPhase::Running; }
void set_match_phase(World& world, MatchPhase phase) { world.match.phase = phase; world.gameOver = phase != MatchPhase::Running; }

void consume_replay_commands(std::vector<ReplayCommand>& out) { out = std::move(gReplayCommands); gReplayCommands.clear(); }

bool players_allied(const World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size()) return a == b;
  return world.players[a].teamId == world.players[b].teamId;
}

int compute_player_score(const World& world, uint16_t playerId) {
  if (playerId >= world.players.size()) return 0;
  const auto& p = world.players[playerId];
  int resources = 0;
  for (float r : p.resources) resources += (int)std::floor(r);
  int unitsAlive = 0;
  int buildingsAlive = 0;
  for (const auto& u : world.units) if (u.team == playerId && u.hp > 0) ++unitsAlive;
  for (const auto& b : world.buildings) if (b.team == playerId && !b.underConstruction && b.hp > 0.0f) ++buildingsAlive;
  const int capitals = controlled_capitals(world, playerId);
  return resources * world.config.scoreResourceWeight +
      unitsAlive * world.config.scoreUnitWeight +
      buildingsAlive * world.config.scoreBuildingWeight +
      ((int)p.age + 1) * world.config.scoreAgeWeight +
      capitals * world.config.scoreCapitalWeight;
}



void apply_world_defaults(World& w) {
  w.players = {{0, Age::Ancient}, {1, Age::Ancient}};
  w.players[0].isHuman = true; w.players[0].isCPU = false; w.players[0].teamId = 0; w.players[0].civilization = civilization_runtime_for("default");
  w.players[1].isHuman = false; w.players[1].isCPU = true; w.players[1].teamId = 1; w.players[1].civilization = civilization_runtime_for("default");
  w.tick = 0;
  w.match = {};
  w.wonder = {};
  w.gameOver = false;
  w.winner = 0;
  w.rejectedCommandCount = 0;
  w.triggerExecutionCount = 0;
  w.objectiveStateChangeCount = 0;
  w.objectiveLog.clear();
  gReplayCommands.clear();
}

bool validate_size(const World& w, const std::vector<float>& v) { return (int)v.size() == w.width * w.height; }

void eval_triggers(World& w) {
  std::sort(w.triggers.begin(), w.triggers.end(), [](const Trigger& a, const Trigger& b){ return a.id < b.id; });
  for (auto& t : w.triggers) {
    if (t.once && t.fired) continue;
    bool hit = false;
    switch (t.condition.type) {
      case TriggerType::TickReached: hit = w.tick >= t.condition.tick; break;
      case TriggerType::EntityDestroyed: {
        bool unitAlive = false; for (const auto& u : w.units) if (u.id == t.condition.entityId && u.hp > 0) unitAlive = true;
        bool buildingAlive = false; for (const auto& b : w.buildings) if (b.id == t.condition.entityId && b.hp > 0) buildingAlive = true;
        hit = !(unitAlive || buildingAlive);
      } break;
      case TriggerType::BuildingCompleted: {
        for (const auto& b : w.buildings) {
          if ((t.condition.player == UINT16_MAX || b.team == t.condition.player) && b.type == t.condition.buildingType && !b.underConstruction) { hit = true; break; }
        }
      } break;
      case TriggerType::AreaEntered: {
        auto it = std::find_if(w.triggerAreas.begin(), w.triggerAreas.end(), [&](const TriggerArea& a){ return a.id == t.condition.areaId; });
        if (it != w.triggerAreas.end()) {
          for (const auto& u : w.units) {
            if (t.condition.player != UINT16_MAX && u.team != t.condition.player) continue;
            if (u.pos.x >= it->min.x && u.pos.x <= it->max.x && u.pos.y >= it->min.y && u.pos.y <= it->max.y) { hit = true; break; }
          }
        }
      } break;
      case TriggerType::PlayerEliminated: {
        if (t.condition.player < w.players.size()) hit = !w.players[t.condition.player].alive;
      } break;
    }
    if (!hit) continue;
    t.fired = true;
    ++w.triggerExecutionCount;
    for (const auto& a : t.actions) {
      if (a.type == TriggerActionType::ShowObjectiveText) w.objectiveLog.push_back({w.tick, a.text});
      else if (a.type == TriggerActionType::SetObjectiveState) {
        for (auto& o : w.objectives) if (o.id == a.objectiveId) { if (o.state != a.objectiveState) ++w.objectiveStateChangeCount; o.state = a.objectiveState; }
      } else if (a.type == TriggerActionType::GrantResources) {
        if (a.player < w.players.size()) for (size_t i = 0; i < a.resources.size(); ++i) w.players[a.player].resources[i] += a.resources[i];
      } else if (a.type == TriggerActionType::SpawnUnits) {
        for (uint32_t i = 0; i < a.spawnCount; ++i) spawn_unit(w, a.player, a.spawnUnitType, {a.spawnPos.x + 0.7f * i, a.spawnPos.y});
      } else if (a.type == TriggerActionType::EndMatchWithVictory) {
        apply_match_end(w, VictoryCondition::Conquest, a.winner, false);
      } else if (a.type == TriggerActionType::EndMatchWithDefeat) {
        apply_match_end(w, VictoryCondition::Conquest, a.winner, false);
      } else if (a.type == TriggerActionType::RevealArea) {
        auto it = std::find_if(w.triggerAreas.begin(), w.triggerAreas.end(), [&](const TriggerArea& ar){ return ar.id == a.areaId; });
        if (it != w.triggerAreas.end()) {
          int minX = std::max(0, (int)std::floor(it->min.x)); int maxX = std::min(w.width-1, (int)std::ceil(it->max.x));
          int minY = std::max(0, (int)std::floor(it->min.y)); int maxY = std::min(w.height-1, (int)std::ceil(it->max.y));
          for (int y=minY;y<=maxY;++y) for (int x=minX;x<=maxX;++x) w.fog[y*w.width+x]=255;
        }
      }
    }
  }
}

void initialize_world(World& w, uint32_t seed) {

  load_defs_once();
  gNav.cache.clear();
  gNav.nextMoveOrder = 1;
  w.navVersion = 1;
  w.seed = seed;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> n(0.f, 1.f);
  w.heightmap.resize(w.width * w.height);
  w.fertility.resize(w.width * w.height);
  w.territoryOwner.resize(w.width * w.height);
  w.fog.assign(w.width * w.height, 0);
  for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
    float h = 0.4f * std::sin(x * 0.08f) + 0.4f * std::cos(y * 0.09f) + 0.2f * n(rng);
    w.heightmap[y * w.width + x] = h;
    w.fertility[y * w.width + x] = std::clamp(1.0f - std::abs(h), 0.1f, 1.0f);
  }

  apply_world_defaults(w);
  w.resourceNodes.clear();
  w.triggerAreas.clear();
  w.objectives.clear();
  w.triggers.clear();
  w.cities = {{1, 0, {20, 20}, 1, true}, {2, 1, {95, 95}, 1, true}};
  w.buildings.clear();
  w.buildings.push_back({1, 0, BuildingType::CityCenter, {20, 20}, gBuildDefs[bidx(BuildingType::CityCenter)].size, false, 1.0f, 0.0f, 2200.0f, 2200.0f, {}});
  w.buildings.push_back({2, 1, BuildingType::CityCenter, {95, 95}, gBuildDefs[bidx(BuildingType::CityCenter)].size, false, 1.0f, 0.0f, 2200.0f, 2200.0f, {}});
  for (int i = 0; i < 3; ++i) {
    spawn_unit(w, 0, UnitType::Worker, {18.0f + i * 0.8f, 24.0f});
    spawn_unit(w, 1, UnitType::Worker, {92.0f + i * 0.8f, 89.0f});
  }
  for (int i = 0; i < 4; ++i) {
    spawn_unit(w, 0, UnitType::Infantry, {17.0f + i * 0.8f, 22.0f});
    spawn_unit(w, 1, UnitType::Infantry, {91.0f + i * 0.8f, 87.0f});
  }
  w.resourceNodes.push_back({1, ResourceNodeType::Forest, {28.0f, 26.0f}, 1500.0f, UINT16_MAX});
  w.resourceNodes.push_back({2, ResourceNodeType::Ore, {86.0f, 82.0f}, 1400.0f, UINT16_MAX});
  recompute_population(w);
  recompute_territory(w);
  recompute_fog(w);
}

bool load_scenario_file(World& w, const std::string& path, uint32_t fallbackSeed, std::string& err) {
  std::ifstream in(path);
  if (!in.good()) { err = "scenario not found"; return false; }
  nlohmann::json j; in >> j;
  if (j.value("schemaVersion", 0u) != 1u) { err = "scenario schema mismatch"; return false; }
  if (j.contains("map")) { w.width = j["map"].value("width", 128); w.height = j["map"].value("height", 128); }
  else { w.width = j.value("mapWidth", 128); w.height = j.value("mapHeight", 128); }
  w.seed = j.value("seed", fallbackSeed);
  initialize_world(w, w.seed);
  if (j.contains("heightmap")) { w.heightmap = j["heightmap"].get<std::vector<float>>(); if (!validate_size(w, w.heightmap)) { err = "heightmap size mismatch"; return false; } }
  if (j.contains("fertility")) { w.fertility = j["fertility"].get<std::vector<float>>(); if (!validate_size(w, w.fertility)) { err = "fertility size mismatch"; return false; } }
  if (j.contains("terrainOverrides")) {
    auto to = j["terrainOverrides"];
    if (to.contains("height") && to["height"].is_array()) { w.heightmap = to["height"].get<std::vector<float>>(); if (!validate_size(w, w.heightmap)) { err = "terrainOverrides.height size mismatch"; return false; } }
    if (to.contains("fertility") && to["fertility"].is_array()) { w.fertility = to["fertility"].get<std::vector<float>>(); if (!validate_size(w, w.fertility)) { err = "terrainOverrides.fertility size mismatch"; return false; } }
  }
  if (j.contains("players")) {
    w.players.clear();
    for (const auto& p : j["players"]) {
      PlayerState ps{}; ps.id = p.value("id", (uint16_t)w.players.size()); ps.age = static_cast<Age>(p.value("age", 0));
      if (p.contains("resources")) ps.resources = p["resources"].get<decltype(ps.resources)>();
      ps.popCap = p.value("popCap", 10);
      ps.isHuman = p.value("isHuman", ps.id == 0);
      ps.isCPU = p.value("isCPU", !ps.isHuman);
      ps.teamId = p.value("team", ps.id);
      if (p.contains("color") && p["color"].is_array() && p["color"].size() >= 3) ps.color = {p["color"][0].get<float>(), p["color"][1].get<float>(), p["color"][2].get<float>()};
      ps.civilization = civilization_runtime_for(p.value("civilization", std::string("default")));
      if (p.contains("startingResources")) {
        auto sr = p["startingResources"];
        auto setR = [&](const char* k, Resource r){ if (sr.contains(k)) ps.resources[ridx(r)] = sr[k].get<float>(); };
        setR("Food", Resource::Food); setR("Wood", Resource::Wood); setR("Metal", Resource::Metal);
        setR("Wealth", Resource::Wealth); setR("Knowledge", Resource::Knowledge); setR("Oil", Resource::Oil);
      }
      w.players.push_back(ps);
    }
  }
  for (auto& p : w.players) if (p.civilization.id.empty()) p.civilization = civilization_runtime_for("default");
  w.cities.clear();
  if (j.contains("cities")) for (const auto& c : j["cities"]) { City cc{}; cc.id=c.value("id",0u); cc.team=c.value("team",0u); cc.pos={c["pos"][0].get<float>(), c["pos"][1].get<float>()}; cc.level=c.value("level",1); cc.capital=c.value("capital",false); w.cities.push_back(cc); }
  w.units.clear();
  if (j.contains("units")) for (const auto& u : j["units"]) { spawn_unit(w, u.value("team",0u), parse_unit(u.value("type", std::string("Infantry"))), {u["pos"][0].get<float>(), u["pos"][1].get<float>()}); }
  w.buildings.clear();
  if (j.contains("buildings")) { uint32_t id=1; for (const auto& b : j["buildings"]) { Building bb{}; bb.id=b.value("id",id++); bb.team=b.value("team",0u); bb.type=parse_building(b.value("type",std::string("House"))); bb.pos={b["pos"][0].get<float>(), b["pos"][1].get<float>()}; bb.size=gBuildDefs[bidx(bb.type)].size; bb.underConstruction=b.value("underConstruction", false); bb.buildProgress=bb.underConstruction?b.value("buildProgress",0.0f):1.0f; bb.buildTime=gBuildDefs[bidx(bb.type)].buildTime; bb.maxHp=(bb.type==BuildingType::CityCenter?2200.0f:1000.0f); bb.hp=b.value("hp", bb.maxHp); w.buildings.push_back(bb);} }
  w.resourceNodes.clear();
  if (j.contains("resourceNodes")) { uint32_t id=1; for (const auto& r : j["resourceNodes"]) { ResourceNode rn{}; rn.id=r.value("id",id++); std::string t=r.value("type",std::string("Forest")); rn.type=(t=="Ore"?ResourceNodeType::Ore:(t=="Farmable"?ResourceNodeType::Farmable:(t=="Ruins"?ResourceNodeType::Ruins:ResourceNodeType::Forest))); rn.pos={r["pos"][0].get<float>(), r["pos"][1].get<float>()}; rn.amount=r.value("amount",1000.0f); rn.owner=r.value("owner",(uint16_t)UINT16_MAX); w.resourceNodes.push_back(rn);} }
  if (j.contains("placements")) {
    const auto& pl = j["placements"];
    if (pl.contains("cities")) for (const auto& c : pl["cities"]) { City cc{}; cc.id=c.value("id",0u); cc.team=c.value("team",0u); cc.pos={c["pos"][0].get<float>(), c["pos"][1].get<float>()}; cc.level=c.value("level",1); cc.capital=c.value("capital",false); w.cities.push_back(cc); }
    if (pl.contains("units")) for (const auto& u : pl["units"]) spawn_unit(w, u.value("team",0u), parse_unit(u.value("type", std::string("Infantry"))), {u["pos"][0].get<float>(), u["pos"][1].get<float>()});
    if (pl.contains("buildings")) for (const auto& b : pl["buildings"]) { Building bb{}; bb.id=b.value("id",(uint32_t)(w.buildings.size()+1)); bb.team=b.value("team",0u); bb.type=parse_building(b.value("type",std::string("House"))); bb.pos={b["pos"][0].get<float>(), b["pos"][1].get<float>()}; bb.size=gBuildDefs[bidx(bb.type)].size; bb.underConstruction=b.value("underConstruction", false); bb.buildProgress=bb.underConstruction?b.value("buildProgress",0.0f):1.0f; bb.buildTime=gBuildDefs[bidx(bb.type)].buildTime; bb.maxHp=(bb.type==BuildingType::CityCenter?2200.0f:1000.0f); bb.hp=b.value("hp", bb.maxHp); w.buildings.push_back(bb); }
    if (pl.contains("resourceNodes")) for (const auto& r : pl["resourceNodes"]) { ResourceNode rn{}; rn.id=r.value("id",(uint32_t)(w.resourceNodes.size()+1)); std::string t=r.value("type",std::string("Forest")); rn.type=(t=="Ore"?ResourceNodeType::Ore:(t=="Farmable"?ResourceNodeType::Farmable:(t=="Ruins"?ResourceNodeType::Ruins:ResourceNodeType::Forest))); rn.pos={r["pos"][0].get<float>(), r["pos"][1].get<float>()}; rn.amount=r.value("amount",1000.0f); rn.owner=r.value("owner",(uint16_t)UINT16_MAX); w.resourceNodes.push_back(rn);} 
  }
  if (j.contains("territoryOwner")) { w.territoryOwner = j["territoryOwner"].get<std::vector<uint16_t>>(); if ((int)w.territoryOwner.size()!=w.width*w.height) { err="territory size mismatch"; return false; } }
  if (j.contains("rules")) { auto rr=j["rules"]; w.config.timeLimitTicks=rr.value("timeLimitTicks", w.config.timeLimitTicks); w.config.wonderHoldTicks=rr.value("wonderHoldTicks", w.config.wonderHoldTicks); w.config.allowConquest=rr.value("allowConquest",true); w.config.allowScore=rr.value("allowScore",true); w.config.allowWonder=rr.value("allowWonder",true); }
  if (j.contains("rulesOverrides")) { auto rr=j["rulesOverrides"]; w.config.timeLimitTicks=rr.value("timeLimit", w.config.timeLimitTicks); if (rr.contains("wonderRules")) w.config.allowWonder = rr["wonderRules"].value("enabled", w.config.allowWonder); if (rr.contains("disabledVictoryTypes")) { for (const auto& d : rr["disabledVictoryTypes"]) { std::string dv=d.get<std::string>(); if (dv=="conquest") w.config.allowConquest=false; if (dv=="score") w.config.allowScore=false; if (dv=="wonder") w.config.allowWonder=false; } } }
  w.triggerAreas.clear();
  if (j.contains("areas")) for (const auto& a : j["areas"]) { TriggerArea ta{}; ta.id=a.value("id",0u); ta.min={a["min"][0].get<float>(),a["min"][1].get<float>()}; ta.max={a["max"][0].get<float>(),a["max"][1].get<float>()}; w.triggerAreas.push_back(ta); }
  w.objectives.clear();
  if (j.contains("objectives")) for (const auto& o : j["objectives"]) { Objective ob{}; ob.id=o.value("id",0u); ob.title=o.value("title",""); ob.text=o.value("text",""); ob.primary=o.value("primary",true); ob.state=parse_objective_state(o.value("state",std::string("inactive"))); ob.owner=o.value("owner",(uint16_t)UINT16_MAX); w.objectives.push_back(ob);}
  w.triggers.clear();
  if (j.contains("triggers")) for (const auto& t : j["triggers"]) { Trigger tr{}; tr.id=t.value("id",0u); tr.once=t.value("once",true); auto c=t["condition"]; std::string ctype=c.value("type",std::string("TickReached")); if (ctype=="EntityDestroyed") tr.condition.type=TriggerType::EntityDestroyed; else if (ctype=="BuildingCompleted") tr.condition.type=TriggerType::BuildingCompleted; else if (ctype=="AreaEntered") tr.condition.type=TriggerType::AreaEntered; else if (ctype=="PlayerEliminated") tr.condition.type=TriggerType::PlayerEliminated; else tr.condition.type=TriggerType::TickReached; tr.condition.tick=c.value("tick",0u); tr.condition.entityId=c.value("entityId",0u); tr.condition.buildingType=parse_building(c.value("buildingType",std::string("House"))); tr.condition.areaId=c.value("areaId",0u); tr.condition.player=c.value("player",(uint16_t)UINT16_MAX); if (t.contains("actions")) for (const auto& a : t["actions"]) { TriggerAction ac{}; std::string at=a.value("type",std::string("ShowObjectiveText")); if (at=="SetObjectiveState") ac.type=TriggerActionType::SetObjectiveState; else if (at=="GrantResources") ac.type=TriggerActionType::GrantResources; else if (at=="SpawnUnits") ac.type=TriggerActionType::SpawnUnits; else if (at=="EndMatchWithVictory") ac.type=TriggerActionType::EndMatchWithVictory; else if (at=="EndMatchWithDefeat") ac.type=TriggerActionType::EndMatchWithDefeat; else if (at=="RevealArea") ac.type=TriggerActionType::RevealArea; else ac.type=TriggerActionType::ShowObjectiveText; ac.text=a.value("text",""); ac.objectiveId=a.value("objectiveId",0u); ac.objectiveState=parse_objective_state(a.value("state",std::string("active"))); ac.player=a.value("player",(uint16_t)UINT16_MAX); if (a.contains("resources")) { auto r=a["resources"]; ac.resources[ridx(Resource::Food)] = r.value("Food",0.0f); ac.resources[ridx(Resource::Wood)] = r.value("Wood",0.0f); ac.resources[ridx(Resource::Metal)] = r.value("Metal",0.0f); ac.resources[ridx(Resource::Wealth)] = r.value("Wealth",0.0f); ac.resources[ridx(Resource::Knowledge)] = r.value("Knowledge",0.0f); ac.resources[ridx(Resource::Oil)] = r.value("Oil",0.0f); } ac.spawnUnitType=parse_unit(a.value("unitType",std::string("Infantry"))); ac.spawnCount=a.value("count",0u); if (a.contains("pos")) ac.spawnPos={a["pos"][0].get<float>(),a["pos"][1].get<float>()}; ac.winner=a.value("winner",0u); ac.areaId=a.value("areaId",0u); tr.actions.push_back(ac);} w.triggers.push_back(tr);}
  recompute_population(w);
  recompute_territory(w);
  recompute_fog(w);
  return true;
}

bool save_scenario_file(const std::string& path, const World& w, std::string& err) {
  nlohmann::json j;
  j["schemaVersion"] = 1; j["seed"] = w.seed; j["mapWidth"] = w.width; j["mapHeight"] = w.height;
  j["players"] = nlohmann::json::array();
  for (const auto& p : w.players) j["players"].push_back({{"id",p.id},{"age",(int)p.age},{"resources",p.resources},{"popCap",p.popCap},{"isHuman",p.isHuman},{"isCPU",p.isCPU},{"team",p.teamId},{"civilization",p.civilization.id},{"color",{p.color[0],p.color[1],p.color[2]}},{"startingResources",{{"Food",p.resources[0]},{"Wood",p.resources[1]},{"Metal",p.resources[2]},{"Wealth",p.resources[3]},{"Knowledge",p.resources[4]},{"Oil",p.resources[5]}}}});
  j["cities"] = nlohmann::json::array(); for (const auto& c : w.cities) j["cities"].push_back({{"id",c.id},{"team",c.team},{"pos",{c.pos.x,c.pos.y}},{"level",c.level},{"capital",c.capital}});
  j["units"] = nlohmann::json::array(); for (const auto& u : w.units) j["units"].push_back({{"id",u.id},{"team",u.team},{"type",unit_name(u.type)},{"pos",{u.pos.x,u.pos.y}}});
  j["buildings"] = nlohmann::json::array(); for (const auto& b : w.buildings) j["buildings"].push_back({{"id",b.id},{"team",b.team},{"type",building_name(b.type)},{"pos",{b.pos.x,b.pos.y}},{"underConstruction",b.underConstruction},{"buildProgress",b.buildProgress},{"hp",b.hp}});
  j["resourceNodes"] = nlohmann::json::array(); for (const auto& r : w.resourceNodes) { std::string t="Forest"; if (r.type==ResourceNodeType::Ore) t="Ore"; else if (r.type==ResourceNodeType::Farmable) t="Farmable"; else if (r.type==ResourceNodeType::Ruins) t="Ruins"; j["resourceNodes"].push_back({{"id",r.id},{"type",t},{"pos",{r.pos.x,r.pos.y}},{"amount",r.amount},{"owner",r.owner}}); }
  j["areas"] = nlohmann::json::array(); for (const auto& a : w.triggerAreas) j["areas"].push_back({{"id",a.id},{"min",{a.min.x,a.min.y}},{"max",{a.max.x,a.max.y}}});
  j["objectives"] = nlohmann::json::array(); for (const auto& o : w.objectives) j["objectives"].push_back({{"id",o.id},{"title",o.title},{"text",o.text},{"primary",o.primary},{"state",objective_state_name(o.state)},{"owner",o.owner}});
  j["triggers"] = nlohmann::json::array();
  for (const auto& t : w.triggers) {
    nlohmann::json jt; jt["id"]=t.id; jt["once"]=t.once;
    std::string ctype="TickReached"; if (t.condition.type==TriggerType::EntityDestroyed) ctype="EntityDestroyed"; else if (t.condition.type==TriggerType::BuildingCompleted) ctype="BuildingCompleted"; else if (t.condition.type==TriggerType::AreaEntered) ctype="AreaEntered"; else if (t.condition.type==TriggerType::PlayerEliminated) ctype="PlayerEliminated";
    jt["condition"]={{"type",ctype},{"tick",t.condition.tick},{"entityId",t.condition.entityId},{"buildingType",building_name(t.condition.buildingType)},{"areaId",t.condition.areaId},{"player",t.condition.player}};
    jt["actions"]=nlohmann::json::array();
    for (const auto& a : t.actions) {
      std::string at="ShowObjectiveText"; if (a.type==TriggerActionType::SetObjectiveState) at="SetObjectiveState"; else if (a.type==TriggerActionType::GrantResources) at="GrantResources"; else if (a.type==TriggerActionType::SpawnUnits) at="SpawnUnits"; else if (a.type==TriggerActionType::EndMatchWithVictory) at="EndMatchWithVictory"; else if (a.type==TriggerActionType::EndMatchWithDefeat) at="EndMatchWithDefeat"; else if (a.type==TriggerActionType::RevealArea) at="RevealArea";
      jt["actions"].push_back({{"type",at},{"text",a.text},{"objectiveId",a.objectiveId},{"state",objective_state_name(a.objectiveState)},{"player",a.player},{"resources",{{"Food",a.resources[0]},{"Wood",a.resources[1]},{"Metal",a.resources[2]},{"Wealth",a.resources[3]},{"Knowledge",a.resources[4]},{"Oil",a.resources[5]}}},{"unitType",unit_name(a.spawnUnitType)},{"count",a.spawnCount},{"pos",{a.spawnPos.x,a.spawnPos.y}},{"winner",a.winner},{"areaId",a.areaId}});
    }
    j["triggers"].push_back(jt);
  }
  std::ofstream of(path);
  if (!of.good()) { err = "cannot write scenario"; return false; }
  of << j.dump(2) << "\n";
  return true;
}


void on_authoritative_state_loaded(World& w) {
  load_defs_once();
  gReplayCommands.clear();
  gNav.cache.clear();
  gNav.nextMoveOrder = 1;
  ++w.navVersion;
  w.territoryDirty = true;
  w.fogDirty = true;
  w.uiBuildMenu = false;
  w.uiTrainMenu = false;
  w.uiResearchMenu = false;
  w.placementActive = false;
  w.gameOver = w.match.phase != MatchPhase::Running;
  w.winner = w.match.winner;
}

void tick_world(World& w, float dt) {
  ++w.tick;
  if (w.match.phase != MatchPhase::Running) {
    if (w.match.phase == MatchPhase::Ended) w.match.phase = MatchPhase::Postmatch;
    return;
  }
  if (w.tick % 10 == 0) recompute_territory(w);

  for (auto& p : w.players) p.resources[ridx(Resource::Food)] += 0.4f * dt * 20.0f;

  for (auto& b : w.buildings) {
    auto& owner = w.players[b.team];
    const BuildDef& def = gBuildDefs[bidx(b.type)];
    if (b.underConstruction) {
      float workerFactor = has_nearby_builder(w, b.team, b.pos) ? 1.0f : 0.25f;
      b.buildProgress += dt / std::max(1.0f, b.buildTime) * workerFactor;
      if (b.buildProgress >= 1.0f) { b.underConstruction = false; ++w.completedBuildingsCount; ++w.navVersion; gNav.cache.clear(); }
      continue;
    }

    float op = has_nearby_builder(w, b.team, b.pos) ? 1.0f : 0.0f;
    for (size_t r = 0; r < static_cast<size_t>(Resource::Count); ++r) {
      float trick = def.trickle[r];
      if (trick > 0.0f) {
        float mult = 1.0f;
        if (b.type == BuildingType::Farm) mult = fertility_at(w, b.pos);
        owner.resources[r] += trick * op * dt * 20.0f;
      }
    }

    if (!b.queue.empty()) {
      b.queue.front().remaining -= dt;
      if (b.queue.front().remaining <= 0.0f) {
        ProductionItem item = b.queue.front();
        b.queue.erase(b.queue.begin());
        if (item.kind == QueueKind::TrainUnit) {
          spawn_unit(w, b.team, item.unitType, {b.pos.x + 1.5f, b.pos.y + 1.0f});
          ++w.trainedUnitsViaQueue;
        } else if (item.kind == QueueKind::AgeResearch) {
          auto& p = w.players[b.team];
          if ((int)p.age < item.targetAge) p.age = static_cast<Age>(item.targetAge);
        }
      }
    }
  }

  for (auto& c : w.cities) {
    int teamBuildings = 0;
    for (const auto& b : w.buildings) if (b.team == c.team && !b.underConstruction) ++teamBuildings;
    c.level = 1 + teamBuildings / 4;
  }

  for (auto& u : w.units) {
    if (u.hp <= 0) continue;
    if (u.attackCooldownTicks > 0) --u.attackCooldownTicks;
    bool engagedThisTick = false;

    Unit* locked = u.targetUnit ? find_unit(w, u.targetUnit) : nullptr;
    const glm::vec2 centroid = group_centroid_for_order(w, u);

    const float aggro = u.attackMove ? (kAttackMoveAggroPermille / 1000.0f) : 7.0f;
    const float chase = u.attackMove ? (kAttackMoveChasePermille / 1000.0f) : 10.0f;

    uint32_t candidate = find_enemy_near(w, u, aggro);
    if (!locked && candidate != 0) {
      u.targetUnit = candidate;
      u.targetLockTicks = 0;
      ++w.targetSwitchCount;
      locked = find_unit(w, candidate);
    } else if (locked && candidate != 0 && candidate != u.targetUnit && u.targetLockTicks >= kTargetLockMinTicks) {
      Unit* better = find_unit(w, candidate);
      if (better) {
        float ddOld = dist(u.pos, locked->pos);
        float ddNew = dist(u.pos, better->pos);
        int oldScore = 5000 - static_cast<int>(ddOld * 800.0f) + static_cast<int>(u.vsRoleMultiplierPermille[role_idx(locked->role)] - 1000);
        int newScore = 5000 - static_cast<int>(ddNew * 800.0f) + static_cast<int>(u.vsRoleMultiplierPermille[role_idx(better->role)] - 1000);
        if (newScore > oldScore + kTargetBetterThreshold) {
          u.targetUnit = better->id;
          u.targetLockTicks = 0;
          ++w.targetSwitchCount;
          locked = better;
        }
      }
    }

    glm::vec2 desired = u.target - u.pos;
    if (locked) {
      u.target = locked->pos;
      const float dd = dist(u.pos, locked->pos);
      const float dc = dist(u.pos, centroid);
      if (dd > chase || (u.attackMove && dc > (kCentroidLeashPermille / 1000.0f))) {
        u.targetUnit = 0;
        u.chaseTicks = 0;
        if (u.attackMove) ++w.chaseLimitBreakCount;
      } else {
        engagedThisTick = true;
        ++u.targetLockTicks;
        ++u.chaseTicks;
      }
    }

    int buildingTarget = -1;
    if (u.role == UnitRole::Siege && (!locked || u.attackMove)) {
      buildingTarget = find_building_target(w, u, aggro + 4.0f);
      if (buildingTarget >= 0 && (!locked || dist(u.pos, w.buildings[buildingTarget].pos) <= u.range + 1.5f)) {
        u.target = w.buildings[buildingTarget].pos;
        engagedThisTick = true;
      }
    }

    if (!engagedThisTick && u.hasMoveOrder && u.moveOrder != 0) {
      if (FlowField* ff = get_flow_field(w, cell_of(w, u.slotTarget))) {
        const int cc = cell_of(w, u.pos);
        desired = glm::vec2{(float)ff->dirX[cc], (float)ff->dirY[cc]};
        if (glm::length(desired) < 0.1f) desired = u.slotTarget - u.pos;
      }
      if (u.orderPathLingerTicks > 0) --u.orderPathLingerTicks;
    } else if (engagedThisTick) {
      u.orderPathLingerTicks = kMaxOrderPathLingerTicks;
      desired = u.target - u.pos;
    }

    glm::vec2 repulse{0.0f, 0.0f};
    for (const auto& other : w.units) {
      if (other.id == u.id || other.team != u.team || other.hp <= 0) continue;
      glm::vec2 delta = u.pos - other.pos;
      float l = glm::length(delta);
      if (l > 0.001f && l < 1.2f) repulse += (delta / l) * (1.2f - l);
    }
    desired += repulse * 0.65f;

    float len = glm::length(desired);
    if (len > 0.05f) {
      glm::vec2 dir = desired / len;
      u.moveDir = glm::mix(u.moveDir, dir, 0.35f);
      float ml = glm::length(u.moveDir);
      if (ml > 0.001f) u.moveDir /= ml;
      glm::vec2 prev = u.pos;
      u.pos += u.moveDir * u.speed * dt;
      if (glm::length(u.pos - prev) < 0.005f && u.hasMoveOrder) {
        u.stuckTicks = (uint16_t)std::min<int>(u.stuckTicks + 1, 65535);
        if (u.stuckTicks > 80) ++w.stuckMoveAssertions;
      } else u.stuckTicks = 0;
    }

    if (u.hasMoveOrder && dist(u.pos, u.slotTarget) < 1.1f && !engagedThisTick) {
      ++w.unitsReachedSlotCount;
      u.hasMoveOrder = false;
      u.moveOrder = 0;
      u.target = u.pos;
      u.attackMove = false;
      u.attackMoveOrder = 0;
    }

    if (u.type != UnitType::Worker && u.attackCooldownTicks == 0) {
      if (locked && dist(u.pos, locked->pos) <= u.range + 0.2f) {
        int mult = (int)u.vsRoleMultiplierPermille[role_idx(locked->role)];
        float damage = u.attack * (mult / 1000.0f);
        locked->hp -= damage;
        w.totalDamageDealtPermille += (uint32_t)(damage * 1000.0f);
        ++w.combatEngagementCount;
        u.attackCooldownTicks = (uint16_t)gUnitDefs[uidx(u.type)].attackCooldownTicks;
      } else if (buildingTarget >= 0 && dist(u.pos, w.buildings[buildingTarget].pos) <= u.range + 0.8f) {
        float mult = (u.role == UnitRole::Siege) ? 1.8f : 0.8f;
        float damage = u.attack * mult;
        w.buildings[buildingTarget].hp -= damage;
        w.totalDamageDealtPermille += (uint32_t)(damage * 1000.0f);
        ++w.buildingDamageEvents;
        ++w.combatEngagementCount;
        u.attackCooldownTicks = (uint16_t)gUnitDefs[uidx(u.type)].attackCooldownTicks;
      }
    }
    if (engagedThisTick) ++w.combatTicks;
    u.renderPos = glm::mix(u.renderPos, u.pos, 0.35f);
  }
  const size_t beforeUnits = w.units.size();
  w.units.erase(std::remove_if(w.units.begin(), w.units.end(), [&](const Unit& u) {
    if (u.hp > 0) return false;
    w.players[u.team].unitsLost += 1;
    return true;
  }), w.units.end());
  w.unitDeathEvents += static_cast<uint32_t>(beforeUnits - w.units.size());

  const size_t beforeBuildings = w.buildings.size();
  w.buildings.erase(std::remove_if(w.buildings.begin(), w.buildings.end(), [&](const Building& b) {
    if (b.hp > 0.0f) return false;
    w.players[b.team].buildingsLost += 1;
    if (b.type == BuildingType::Wonder) { w.wonder.owner = std::numeric_limits<uint16_t>::max(); w.wonder.heldTicks = 0; }
    ++w.navVersion;
    return true;
  }), w.buildings.end());
  if (beforeBuildings != w.buildings.size()) gNav.cache.clear();

  for (auto& p : w.players) p.finalScore = compute_player_score(w, p.id);

  bool tieBreakUsed = false;
  int alivePlayers = 0;
  uint16_t aliveId = 0;
  for (auto& p : w.players) {
    p.alive = controlled_capitals(w, p.id) > 0;
    if (p.alive) { ++alivePlayers; aliveId = p.id; }
  }
  if (w.config.allowConquest && alivePlayers == 1) apply_match_end(w, VictoryCondition::Conquest, aliveId, false);

  uint16_t wonderOwner = std::numeric_limits<uint16_t>::max();
  for (const auto& b : w.buildings) if (b.type == BuildingType::Wonder && !b.underConstruction && b.hp > 0.0f) { wonderOwner = b.team; break; }
  if (wonderOwner == std::numeric_limits<uint16_t>::max()) {
    w.wonder.owner = wonderOwner;
    w.wonder.heldTicks = 0;
  } else {
    if (w.wonder.owner != wonderOwner) { w.wonder.owner = wonderOwner; w.wonder.heldTicks = 0; }
    else ++w.wonder.heldTicks;
    if (w.config.allowWonder && w.wonder.heldTicks >= w.config.wonderHoldTicks) apply_match_end(w, VictoryCondition::Wonder, wonderOwner, false);
  }

  if (w.config.allowScore && w.tick >= w.config.timeLimitTicks && w.match.phase == MatchPhase::Running) {
    int bestScore = std::numeric_limits<int>::min();
    uint16_t bestId = 0;
    for (const auto& p : w.players) {
      int score = p.finalScore;
      if (score > bestScore || (score == bestScore && p.id < bestId)) {
        tieBreakUsed = (score == bestScore && p.id < bestId);
        bestScore = score;
        bestId = p.id;
      }
    }
    apply_match_end(w, VictoryCondition::Score, bestId, tieBreakUsed);
  }

  eval_triggers(w);
  recompute_population(w);
  recompute_fog(w);

  if (w.match.phase == MatchPhase::Ended) w.match.phase = MatchPhase::Postmatch;
}

void issue_move(World& w, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target) {
  if (!gameplay_orders_allowed(w)) { ++w.rejectedCommandCount; return; }
  gReplayCommands.push_back({ReplayCommandType::Move, w.tick, team, ids, target});
  std::vector<uint32_t> sorted = ids;
  std::sort(sorted.begin(), sorted.end());
  const int n = (int)sorted.size();
  const int cols = std::max(1, (int)std::ceil(std::sqrt((float)n)));
  const int rows = (n + cols - 1) / cols;
  const float spacing = 1.1f;
  const uint32_t moveOrder = gNav.nextMoveOrder++;
  if (n >= 6) ++w.groupMoveCommandCount;
  (void)get_flow_field(w, cell_of(w, target));

  for (int i = 0; i < n; ++i) {
    const uint32_t id = sorted[i];
    const int r = i / cols;
    const int c = i % cols;
    glm::vec2 slot = target + glm::vec2{(c - cols * 0.5f) * spacing, (r - rows * 0.5f) * spacing};
    for (auto& u : w.units) if (u.team == team && u.id == id) {
      u.slotTarget = slot;
      u.target = slot;
      u.targetUnit = 0;
      u.moveOrder = moveOrder;
      u.hasMoveOrder = true;
      u.stuckTicks = 0;
      u.attackMove = false;
      u.attackMoveOrder = 0;
      u.chaseTicks = 0;
      u.orderPathLingerTicks = 0;
      break;
    }
  }
}

void issue_attack(World& w, uint16_t team, const std::vector<uint32_t>& ids, uint32_t enemy) {
  if (!gameplay_orders_allowed(w)) { ++w.rejectedCommandCount; return; }
  ReplayCommand cmd{}; cmd.type = ReplayCommandType::Attack; cmd.tick = w.tick; cmd.team = team; cmd.ids = ids; cmd.enemy = enemy; gReplayCommands.push_back(cmd);
  for (auto& u : w.units) if (u.team == team && std::find(ids.begin(), ids.end(), u.id) != ids.end()) {
    u.targetUnit = enemy;
    u.attackMove = false;
    u.attackMoveOrder = 0;
  }
}

void issue_attack_move(World& w, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target) {
  if (!gameplay_orders_allowed(w)) { ++w.rejectedCommandCount; return; }
  gReplayCommands.push_back({ReplayCommandType::AttackMove, w.tick, team, ids, target});
  issue_move(w, team, ids, target);
  for (auto& u : w.units) if (u.team == team && std::find(ids.begin(), ids.end(), u.id) != ids.end()) {
    u.attackMove = true;
    u.attackMoveOrder = u.moveOrder;
  }
}

void toggle_god_mode(World& w) { w.godMode = !w.godMode; w.fogDirty = true; }

void set_nav_debug(bool enabled) { gNavDebug = enabled; }
bool nav_debug_enabled() { return gNavDebug; }
void set_combat_debug(bool enabled) { gCombatDebug = enabled; }
bool combat_debug_enabled() { return gCombatDebug; }

bool start_build_placement(World& world, uint16_t, BuildingType type) {
  if (!gameplay_orders_allowed(world)) { ++world.rejectedCommandCount; return false; }
  world.placementActive = true; world.placementType = type; return true;
}
void update_build_placement(World& world, uint16_t team, glm::vec2 worldPos) {
  world.placementPos = worldPos;
  world.placementValid = placeable(world, team, world.placementType, worldPos) && can_afford(world.players[team].resources, gBuildDefs[bidx(world.placementType)].cost);
}
bool confirm_build_placement(World& world, uint16_t team) {
  if (!gameplay_orders_allowed(world)) { ++world.rejectedCommandCount; return false; }
  if (!world.placementActive || !placeable(world, team, world.placementType, world.placementPos)) return false;
  if (!spend(world.players[team].resources, gBuildDefs[bidx(world.placementType)].cost)) return false;
  uint32_t id = 1; for (const auto& b : world.buildings) id = std::max(id, b.id + 1);
  const auto& d = gBuildDefs[bidx(world.placementType)];
  world.buildings.push_back({id, team, world.placementType, world.placementPos, d.size, true, 0.0f, d.buildTime, 1000.0f, 1000.0f, {}});
  ReplayCommand cmd{}; cmd.type = ReplayCommandType::PlaceBuilding; cmd.tick = world.tick; cmd.team = team; cmd.target = world.placementPos; cmd.buildingType = world.placementType; gReplayCommands.push_back(cmd);
  ++world.navVersion;
  gNav.cache.clear();
  world.placementActive = false;
  return true;
}
void cancel_build_placement(World& world) { world.placementActive = false; }

bool enqueue_train_unit(World& world, uint16_t team, uint32_t buildingId, UnitType type) {
  if (!gameplay_orders_allowed(world)) { ++world.rejectedCommandCount; return false; }
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team && !b.underConstruction; });
  if (it == world.buildings.end()) return false;
  if (it->type == BuildingType::CityCenter && type != UnitType::Worker) return false;
  if (it->type == BuildingType::Barracks && type == UnitType::Worker) return false;
  auto& p = world.players[team];
  if (p.popUsed + (int)it->queue.size() + gUnitDefs[uidx(type)].popCost > p.popCap) return false;
  if (!spend(p.resources, gUnitDefs[uidx(type)].cost)) return false;
  it->queue.push_back({QueueKind::TrainUnit, type, gUnitDefs[uidx(type)].trainTime, 0});
  ReplayCommand cmd{}; cmd.type = ReplayCommandType::QueueTrain; cmd.tick = world.tick; cmd.team = team; cmd.buildingId = buildingId; cmd.unitType = type; gReplayCommands.push_back(cmd);
  return true;
}

bool enqueue_age_research(World& world, uint16_t team, uint32_t buildingId) {
  if (!gameplay_orders_allowed(world)) { ++world.rejectedCommandCount; return false; }
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team && !b.underConstruction; });
  if (it == world.buildings.end()) return false;
  if (!(it->type == BuildingType::Library || it->type == BuildingType::CityCenter)) return false;
  auto& p = world.players[team];
  if (p.age >= Age::Information) return false;
  bool pendingAge = std::any_of(it->queue.begin(), it->queue.end(), [](const ProductionItem& q) { return q.kind == QueueKind::AgeResearch; });
  if (pendingAge) return false;
  if (!spend(p.resources, gAgeResearchCost)) return false;
  it->queue.push_back({QueueKind::AgeResearch, UnitType::Worker, gAgeResearchTime, (int)p.age + 1});
  ReplayCommand cmd{}; cmd.type = ReplayCommandType::QueueResearch; cmd.tick = world.tick; cmd.team = team; cmd.buildingId = buildingId; gReplayCommands.push_back(cmd);
  ++world.researchStartedCount;
  return true;
}

bool cancel_queue_item(World& world, uint16_t team, uint32_t buildingId, size_t index) {
  if (!gameplay_orders_allowed(world)) { ++world.rejectedCommandCount; return false; }
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team; });
  if (it == world.buildings.end() || index >= it->queue.size()) return false;
  auto& p = world.players[team];
  if (it->queue[index].kind == QueueKind::TrainUnit) refund(p.resources, gUnitDefs[uidx(it->queue[index].unitType)].cost, 0.8f);
  else refund(p.resources, gAgeResearchCost, 0.8f);
  it->queue.erase(it->queue.begin() + (long)index);
  ReplayCommand cmd{}; cmd.type = ReplayCommandType::CancelQueue; cmd.tick = world.tick; cmd.team = team; cmd.buildingId = buildingId; cmd.queueIndex = index; gReplayCommands.push_back(cmd);
  return true;
}

uint64_t map_setup_hash(const World& w) {
  uint64_t h = kFNVOffset;
  for (float v : w.heightmap) hash_float(h, v);
  for (float v : w.fertility) hash_float(h, v);
  for (const auto& c : w.cities) { hash_u32(h, c.id); hash_u32(h, c.team); hash_float(h, c.pos.x); hash_float(h, c.pos.y); }
  for (const auto& b : w.buildings) { hash_u32(h, b.id); hash_u32(h, b.team); hash_u32(h, (uint32_t)b.type); hash_float(h, b.pos.x); hash_float(h, b.pos.y); }
  for (const auto& u : w.units) { hash_u32(h, u.id); hash_u32(h, u.team); hash_u32(h, (uint32_t)u.type); hash_float(h, u.pos.x); hash_float(h, u.pos.y); }
  for (const auto& r : w.resourceNodes) { hash_u32(h, r.id); hash_u32(h, (uint32_t)r.type); hash_float(h, r.pos.x); hash_float(h, r.pos.y); hash_float(h, r.amount); }
  for (const auto& o : w.objectives) { hash_u32(h, o.id); hash_u32(h, (uint32_t)o.state); }
  return h;
}

uint64_t state_hash(const World& w) {
  uint64_t h = kFNVOffset;
  hash_u32(h, w.tick);
  hash_u32(h, static_cast<uint32_t>(w.units.size()));
  hash_u32(h, static_cast<uint32_t>(w.buildings.size()));
  for (const auto& u : w.units) { hash_u32(h, u.id); hash_float(h, u.hp); hash_float(h, u.pos.x); hash_float(h, u.pos.y); }
  for (const auto& b : w.buildings) {
    hash_u32(h, b.id); hash_u32(h, (uint32_t)b.type); hash_u32(h, b.underConstruction ? 1 : 0); hash_float(h, b.buildProgress);
    hash_u32(h, (uint32_t)b.queue.size());
    for (const auto& q : b.queue) { hash_u32(h, (uint32_t)q.kind); hash_u32(h, (uint32_t)q.unitType); hash_float(h, q.remaining); hash_u32(h, q.targetAge); }
  }
  for (const auto& p : w.players) {
    hash_u32(h, (uint32_t)p.age); hash_u32(h, (uint32_t)p.popUsed); hash_u32(h, (uint32_t)p.popCap);
    hash_u32(h, p.unitsLost); hash_u32(h, p.buildingsLost); hash_u32(h, (uint32_t)p.finalScore);
    for (float r : p.resources) hash_float(h, r);
  }
  hash_u32(h, static_cast<uint32_t>(w.match.phase));
  hash_u32(h, static_cast<uint32_t>(w.match.condition));
  hash_u32(h, w.match.winner);
  hash_u32(h, w.match.endTick);
  hash_u32(h, w.wonder.owner == std::numeric_limits<uint16_t>::max() ? 0xFFFFu : w.wonder.owner);
  hash_u32(h, w.wonder.heldTicks);
  hash_u32(h, w.territoryRecomputeCount);
  hash_u32(h, w.aiDecisionCount);
  hash_u32(h, w.completedBuildingsCount);
  hash_u32(h, w.trainedUnitsViaQueue);
  hash_u32(h, w.triggerExecutionCount);
  hash_u32(h, w.objectiveStateChangeCount);
  for (const auto& o : w.objectives) { hash_u32(h, o.id); hash_u32(h, (uint32_t)o.state); }
  for (const auto& l : w.objectiveLog) { hash_u32(h, l.tick); hash_u32(h, (uint32_t)l.text.size()); }
  return h;
}

} // namespace dom::sim
