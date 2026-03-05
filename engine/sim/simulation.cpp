#include "engine/sim/simulation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
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
};

BuildDef gBuildDefs[8];
UnitDef gUnitDefs[2];
float gAgeResearchTime{30.0f};
std::array<float, static_cast<size_t>(Resource::Count)> gAgeResearchCost{};
bool gDefsLoaded{false};

size_t ridx(Resource r) { return static_cast<size_t>(r); }
int bidx(BuildingType t) { return static_cast<int>(t); }
int uidx(UnitType t) { return static_cast<int>(t); }

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

  gUnitDefs[uidx(UnitType::Worker)].trainTime = 10.0f;
  gUnitDefs[uidx(UnitType::Worker)].cost[ridx(Resource::Food)] = 60;
  gUnitDefs[uidx(UnitType::Worker)].popCost = 1;
  gUnitDefs[uidx(UnitType::Infantry)].trainTime = 12.0f;
  gUnitDefs[uidx(UnitType::Infantry)].cost[ridx(Resource::Food)] = 70;
  gUnitDefs[uidx(UnitType::Infantry)].cost[ridx(Resource::Metal)] = 30;
  gUnitDefs[uidx(UnitType::Infantry)].popCost = 1;

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
      UnitType t = (id == "Worker") ? UnitType::Worker : UnitType::Infantry;
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

uint32_t spawn_unit(World& w, uint16_t team, UnitType type, glm::vec2 p) {
  uint32_t id = 1;
  for (const auto& u : w.units) id = std::max(id, u.id + 1);
  Unit nu{}; nu.id = id; nu.team = team; nu.type = type; nu.pos = nu.renderPos = nu.target = p;
  if (type == UnitType::Worker) { nu.hp = 70; nu.attack = 3.0f; nu.range = 1.5f; nu.speed = 4.3f; }
  else { nu.hp = 100; nu.attack = 8.5f; nu.range = 2.7f; nu.speed = 4.8f; }
  w.units.push_back(nu);
  return id;
}

} // namespace

void initialize_world(World& w, uint32_t seed) {
  load_defs_once();
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

  w.players = {{0, Age::Ancient}, {1, Age::Ancient}};
  w.cities = {{1, 0, {20, 20}, 1, true}, {2, 1, {95, 95}, 1, true}};
  w.buildings.clear();
  w.buildings.push_back({1, 0, BuildingType::CityCenter, {20, 20}, gBuildDefs[bidx(BuildingType::CityCenter)].size, false, 1.0f, 0.0f, {}});
  w.buildings.push_back({2, 1, BuildingType::CityCenter, {95, 95}, gBuildDefs[bidx(BuildingType::CityCenter)].size, false, 1.0f, 0.0f, {}});
  for (int i = 0; i < 3; ++i) {
    spawn_unit(w, 0, UnitType::Worker, {18.0f + i * 0.8f, 24.0f});
    spawn_unit(w, 1, UnitType::Worker, {92.0f + i * 0.8f, 89.0f});
  }
  for (int i = 0; i < 4; ++i) {
    spawn_unit(w, 0, UnitType::Infantry, {17.0f + i * 0.8f, 22.0f});
    spawn_unit(w, 1, UnitType::Infantry, {91.0f + i * 0.8f, 87.0f});
  }
  recompute_population(w);
  recompute_territory(w);
  recompute_fog(w);
}

void tick_world(World& w, float dt) {
  ++w.tick;
  if (w.tick % 10 == 0) recompute_territory(w);

  for (auto& p : w.players) p.resources[ridx(Resource::Food)] += 0.4f * dt * 20.0f;

  for (auto& b : w.buildings) {
    auto& owner = w.players[b.team];
    const BuildDef& def = gBuildDefs[bidx(b.type)];
    if (b.underConstruction) {
      float workerFactor = has_nearby_builder(w, b.team, b.pos) ? 1.0f : 0.25f;
      b.buildProgress += dt / std::max(1.0f, b.buildTime) * workerFactor;
      if (b.buildProgress >= 1.0f) { b.underConstruction = false; ++w.completedBuildingsCount; }
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
    if (u.targetUnit != 0) {
      auto it = std::find_if(w.units.begin(), w.units.end(), [&](const Unit& e) { return e.id == u.targetUnit && e.hp > 0; });
      if (it != w.units.end()) u.target = it->pos;
    }

    glm::vec2 d = u.target - u.pos;
    float len = glm::length(d);
    if (len > 0.1f) u.pos += (d / len) * u.speed * dt;

    if (u.type != UnitType::Worker) {
      for (auto& e : w.units) {
        if (e.team == u.team || e.hp <= 0) continue;
        float dd = dist(u.pos, e.pos);
        if (dd <= u.range) {
          float mult = 1.0f;
          int tx = std::clamp((int)u.pos.x, 0, w.width - 1), ty = std::clamp((int)u.pos.y, 0, w.height - 1);
          if (!w.godMode && w.territoryOwner[ty * w.width + tx] == e.team) mult = 0.85f;
          e.hp -= u.attack * mult * dt;
        }
      }
    }
    u.renderPos = glm::mix(u.renderPos, u.pos, 0.35f);
  }

  w.units.erase(std::remove_if(w.units.begin(), w.units.end(), [](const Unit& u) { return u.hp <= 0; }), w.units.end());
  recompute_population(w);
  recompute_fog(w);

  if (w.tick > 20 * 600) { w.gameOver = true; w.winner = w.players[0].score >= w.players[1].score ? 0 : 1; }
}

void issue_move(World& w, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target) {
  for (auto& u : w.units) if (u.team == team && std::find(ids.begin(), ids.end(), u.id) != ids.end()) { u.target = target; u.targetUnit = 0; }
}

void issue_attack(World& w, uint16_t team, const std::vector<uint32_t>& ids, uint32_t enemy) {
  for (auto& u : w.units) if (u.team == team && std::find(ids.begin(), ids.end(), u.id) != ids.end()) u.targetUnit = enemy;
}

void toggle_god_mode(World& w) { w.godMode = !w.godMode; w.fogDirty = true; }

bool start_build_placement(World& world, uint16_t, BuildingType type) {
  world.placementActive = true; world.placementType = type; return true;
}
void update_build_placement(World& world, uint16_t team, glm::vec2 worldPos) {
  world.placementPos = worldPos;
  world.placementValid = placeable(world, team, world.placementType, worldPos) && can_afford(world.players[team].resources, gBuildDefs[bidx(world.placementType)].cost);
}
bool confirm_build_placement(World& world, uint16_t team) {
  if (!world.placementActive || !placeable(world, team, world.placementType, world.placementPos)) return false;
  if (!spend(world.players[team].resources, gBuildDefs[bidx(world.placementType)].cost)) return false;
  uint32_t id = 1; for (const auto& b : world.buildings) id = std::max(id, b.id + 1);
  const auto& d = gBuildDefs[bidx(world.placementType)];
  world.buildings.push_back({id, team, world.placementType, world.placementPos, d.size, true, 0.0f, d.buildTime, {}});
  world.placementActive = false;
  return true;
}
void cancel_build_placement(World& world) { world.placementActive = false; }

bool enqueue_train_unit(World& world, uint16_t team, uint32_t buildingId, UnitType type) {
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team && !b.underConstruction; });
  if (it == world.buildings.end()) return false;
  if ((it->type == BuildingType::CityCenter && type != UnitType::Worker) || (it->type == BuildingType::Barracks && type != UnitType::Infantry)) return false;
  auto& p = world.players[team];
  if (p.popUsed + (int)it->queue.size() + gUnitDefs[uidx(type)].popCost > p.popCap) return false;
  if (!spend(p.resources, gUnitDefs[uidx(type)].cost)) return false;
  it->queue.push_back({QueueKind::TrainUnit, type, gUnitDefs[uidx(type)].trainTime, 0});
  return true;
}

bool enqueue_age_research(World& world, uint16_t team, uint32_t buildingId) {
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team && !b.underConstruction; });
  if (it == world.buildings.end()) return false;
  if (!(it->type == BuildingType::Library || it->type == BuildingType::CityCenter)) return false;
  auto& p = world.players[team];
  if (p.age >= Age::Information) return false;
  bool pendingAge = std::any_of(it->queue.begin(), it->queue.end(), [](const ProductionItem& q) { return q.kind == QueueKind::AgeResearch; });
  if (pendingAge) return false;
  if (!spend(p.resources, gAgeResearchCost)) return false;
  it->queue.push_back({QueueKind::AgeResearch, UnitType::Worker, gAgeResearchTime, (int)p.age + 1});
  ++world.researchStartedCount;
  return true;
}

bool cancel_queue_item(World& world, uint16_t team, uint32_t buildingId, size_t index) {
  auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [&](const Building& b) { return b.id == buildingId && b.team == team; });
  if (it == world.buildings.end() || index >= it->queue.size()) return false;
  auto& p = world.players[team];
  if (it->queue[index].kind == QueueKind::TrainUnit) refund(p.resources, gUnitDefs[uidx(it->queue[index].unitType)].cost, 0.8f);
  else refund(p.resources, gAgeResearchCost, 0.8f);
  it->queue.erase(it->queue.begin() + (long)index);
  return true;
}

uint64_t map_setup_hash(const World& w) {
  uint64_t h = kFNVOffset;
  for (float v : w.heightmap) hash_float(h, v);
  for (float v : w.fertility) hash_float(h, v);
  for (const auto& c : w.cities) { hash_u32(h, c.id); hash_u32(h, c.team); hash_float(h, c.pos.x); hash_float(h, c.pos.y); }
  for (const auto& b : w.buildings) { hash_u32(h, b.id); hash_u32(h, b.team); hash_u32(h, (uint32_t)b.type); hash_float(h, b.pos.x); hash_float(h, b.pos.y); }
  for (const auto& u : w.units) { hash_u32(h, u.id); hash_u32(h, u.team); hash_u32(h, (uint32_t)u.type); hash_float(h, u.pos.x); hash_float(h, u.pos.y); }
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
    for (float r : p.resources) hash_float(h, r);
  }
  hash_u32(h, w.territoryRecomputeCount);
  hash_u32(h, w.aiDecisionCount);
  hash_u32(h, w.completedBuildingsCount);
  hash_u32(h, w.trainedUnitsViaQueue);
  return h;
}

} // namespace dom::sim
