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
#include <chrono>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <nlohmann/json.hpp>
#ifdef DOM_HAS_LUA
#include <lua.hpp>
#endif

namespace dom::sim {
namespace {
float dist(glm::vec2 a, glm::vec2 b) { return glm::length(a - b); }

size_t dip_index(const World& w, uint16_t a, uint16_t b) {
  const size_t n = w.players.size();
  return static_cast<size_t>(a) * n + static_cast<size_t>(b);
}

DiplomacyRelation relation_of(const World& w, uint16_t a, uint16_t b) {
  if (a >= w.players.size() || b >= w.players.size()) return a == b ? DiplomacyRelation::Allied : DiplomacyRelation::Neutral;
  if (a == b) return DiplomacyRelation::Allied;
  if (w.diplomacy.size() != w.players.size() * w.players.size()) return DiplomacyRelation::Neutral;
  return w.diplomacy[dip_index(w, a, b)];
}

DiplomacyTreaty treaty_of(const World& w, uint16_t a, uint16_t b) {
  if (a >= w.players.size() || b >= w.players.size() || w.treaties.size() != w.players.size() * w.players.size()) return {};
  return w.treaties[dip_index(w, a, b)];
}

void set_relation(World& w, uint16_t a, uint16_t b, DiplomacyRelation rel) {
  if (a >= w.players.size() || b >= w.players.size()) return;
  if (w.diplomacy.size() != w.players.size() * w.players.size()) w.diplomacy.assign(w.players.size() * w.players.size(), DiplomacyRelation::Neutral);
  w.diplomacy[dip_index(w, a, b)] = rel;
  w.diplomacy[dip_index(w, b, a)] = rel;
}

void set_treaty(World& w, uint16_t a, uint16_t b, const DiplomacyTreaty& treaty) {
  if (a >= w.players.size() || b >= w.players.size()) return;
  if (w.treaties.size() != w.players.size() * w.players.size()) w.treaties.assign(w.players.size() * w.players.size(), DiplomacyTreaty{});
  w.treaties[dip_index(w, a, b)] = treaty;
  w.treaties[dip_index(w, b, a)] = treaty;
}

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
  std::array<uint16_t, static_cast<size_t>(UnitRole::Count)> vsRoleMultiplierPermille{1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
  int attackCooldownTicks{12};
  int buildingHp{1000};
};

BuildDef gBuildDefs[static_cast<int>(BuildingType::Count)];
UnitDef gUnitDefs[static_cast<int>(UnitType::Count)];
float gAgeResearchTime{30.0f};
std::array<float, static_cast<size_t>(Resource::Count)> gAgeResearchCost{};
bool gDefsLoaded{false};
bool gNavDebug{false};
bool gCombatDebug{false};
std::vector<ReplayCommand> gReplayCommands;
std::vector<GameplayEvent> gGameplayEvents;
int gWorkerThreads = 1;

GuardianSiteType parse_guardian_site_type(const std::string& v) {
  if (v == "abyssal_trench") return GuardianSiteType::AbyssalTrench;
  if (v == "dune_nest") return GuardianSiteType::DuneNest;
  if (v == "sacred_grove") return GuardianSiteType::SacredGrove;
  if (v == "frozen_cavern") return GuardianSiteType::FrozenCavern;
  return GuardianSiteType::YetiLair;
}

const char* guardian_site_type_name(GuardianSiteType t) {
  switch (t) {
    case GuardianSiteType::YetiLair: return "yeti_lair";
    case GuardianSiteType::AbyssalTrench: return "abyssal_trench";
    case GuardianSiteType::DuneNest: return "dune_nest";
    case GuardianSiteType::SacredGrove: return "sacred_grove";
    case GuardianSiteType::FrozenCavern: return "frozen_cavern";
  }
  return "yeti_lair";
}

GuardianSpawnMode parse_guardian_spawn_mode(const std::string& v) { return v == "scenario_start" ? GuardianSpawnMode::ScenarioStart : GuardianSpawnMode::OnDiscovery; }
GuardianDiscoveryMode parse_guardian_discovery_mode(const std::string& v) {
  if (v == "fog_reveal") return GuardianDiscoveryMode::FogReveal;
  if (v == "underground_discovery") return GuardianDiscoveryMode::UndergroundDiscovery;
  if (v == "scripted_reveal") return GuardianDiscoveryMode::ScriptedReveal;
  return GuardianDiscoveryMode::Proximity;
}
GuardianBehaviorMode parse_guardian_behavior_mode(const std::string& v) {
  if (v == "neutral_encounter") return GuardianBehaviorMode::NeutralEncounter;
  if (v == "hostile_encounter") return GuardianBehaviorMode::HostileEncounter;
  return GuardianBehaviorMode::DormantUntilDiscovery;
}
GuardianJoinMode parse_guardian_join_mode(const std::string& v) {
  if (v == "remain_neutral") return GuardianJoinMode::RemainNeutral;
  if (v == "never_join") return GuardianJoinMode::NeverJoin;
  return GuardianJoinMode::DiscovererControl;
}

const char* guardian_spawn_mode_name(GuardianSpawnMode m) { return m == GuardianSpawnMode::ScenarioStart ? "scenario_start" : "on_discovery"; }
const char* guardian_discovery_mode_name(GuardianDiscoveryMode m) {
  switch (m) {
    case GuardianDiscoveryMode::Proximity: return "proximity";
    case GuardianDiscoveryMode::FogReveal: return "fog_reveal";
    case GuardianDiscoveryMode::UndergroundDiscovery: return "underground_discovery";
    case GuardianDiscoveryMode::ScriptedReveal: return "scripted_reveal";
  }
  return "proximity";
}
const char* guardian_behavior_mode_name(GuardianBehaviorMode m) {
  switch (m) {
    case GuardianBehaviorMode::DormantUntilDiscovery: return "dormant_until_discovery";
    case GuardianBehaviorMode::NeutralEncounter: return "neutral_encounter";
    case GuardianBehaviorMode::HostileEncounter: return "hostile_encounter";
  }
  return "dormant_until_discovery";
}
const char* guardian_join_mode_name(GuardianJoinMode m) {
  switch (m) {
    case GuardianJoinMode::DiscovererControl: return "discoverer_control";
    case GuardianJoinMode::RemainNeutral: return "remain_neutral";
    case GuardianJoinMode::NeverJoin: return "never_join";
  }
  return "discoverer_control";
}

const GuardianDefinition* find_guardian_definition(const World& w, const std::string& id) {
  auto it = std::find_if(w.guardianDefinitions.begin(), w.guardianDefinitions.end(), [&](const GuardianDefinition& d){ return d.guardianId == id; });
  return it == w.guardianDefinitions.end() ? nullptr : &(*it);
}

int find_guardian_site_index(const World& w, uint32_t siteInstanceId) {
  for (size_t i = 0; i < w.guardianSites.size(); ++i) if (w.guardianSites[i].instanceId == siteInstanceId) return static_cast<int>(i);
  return -1;
}

#ifdef DOM_HAS_LUA
lua_State* gLuaState = nullptr;
World* gLuaWorld = nullptr;

int lua_activate_objective(lua_State* L) {
  if (!gLuaWorld) return 0;
  uint32_t oid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  for (auto& o : gLuaWorld->objectives) if (o.id == oid) o.state = ObjectiveState::Active;
  return 0;
}
int lua_complete_objective(lua_State* L) {
  if (!gLuaWorld) return 0;
  uint32_t oid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  for (auto& o : gLuaWorld->objectives) if (o.id == oid) o.state = ObjectiveState::Completed;
  return 0;
}
int lua_fail_objective(lua_State* L) {
  if (!gLuaWorld) return 0;
  uint32_t oid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  for (auto& o : gLuaWorld->objectives) if (o.id == oid) o.state = ObjectiveState::Failed;
  return 0;
}
int lua_show_message(lua_State* L) { if (!gLuaWorld) return 0; gLuaWorld->objectiveLog.push_back({gLuaWorld->tick, luaL_checkstring(L,1)}); return 0; }
int lua_get_tick(lua_State* L) { lua_pushinteger(L, gLuaWorld ? (lua_Integer)gLuaWorld->tick : 0); return 1; }
int lua_get_world_tension(lua_State* L) { lua_pushnumber(L, gLuaWorld ? gLuaWorld->worldTension : 0.0); return 1; }
int lua_get_player_alive(lua_State* L) {
  if (!gLuaWorld) { lua_pushboolean(L,0); return 1; }
  uint16_t p = static_cast<uint16_t>(luaL_checkinteger(L,1));
  lua_pushboolean(L, p < gLuaWorld->players.size() && gLuaWorld->players[p].alive);
  return 1;
}
int lua_get_objective_state(lua_State* L) {
  if (!gLuaWorld) { lua_pushinteger(L,0); return 1; }
  uint32_t oid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  for (const auto& o : gLuaWorld->objectives) if (o.id == oid) { lua_pushinteger(L, (int)o.state); return 1; }
  lua_pushinteger(L, 0); return 1;
}
int lua_activate_guardian_site(lua_State* L) {
  if (!gLuaWorld) return 0;
  activate_guardian_site(*gLuaWorld, static_cast<uint32_t>(luaL_checkinteger(L, 1)));
  return 0;
}
int lua_reveal_guardian_site(lua_State* L) {
  if (!gLuaWorld) return 0;
  const uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  const uint16_t discoverer = static_cast<uint16_t>(luaL_optinteger(L, 2, 0));
  reveal_guardian_site(*gLuaWorld, id, discoverer);
  return 0;
}
int lua_assign_guardian_owner(lua_State* L) {
  if (!gLuaWorld) return 0;
  assign_guardian_owner(*gLuaWorld, static_cast<uint32_t>(luaL_checkinteger(L, 1)), static_cast<uint16_t>(luaL_checkinteger(L, 2)));
  return 0;
}
void ensure_lua(World& w) {
  if (gLuaState) return;
  gLuaState = luaL_newstate();
  luaL_openlibs(gLuaState);
  lua_pushnil(gLuaState); lua_setglobal(gLuaState, "io");
  lua_pushnil(gLuaState); lua_setglobal(gLuaState, "os");
  lua_pushnil(gLuaState); lua_setglobal(gLuaState, "package");
  lua_register(gLuaState, "activate_objective", lua_activate_objective);
  lua_register(gLuaState, "complete_objective", lua_complete_objective);
  lua_register(gLuaState, "fail_objective", lua_fail_objective);
  lua_register(gLuaState, "show_message", lua_show_message);
  lua_register(gLuaState, "get_tick", lua_get_tick);
  lua_register(gLuaState, "get_objective_state", lua_get_objective_state);
  lua_register(gLuaState, "get_player_alive", lua_get_player_alive);
  lua_register(gLuaState, "get_world_tension", lua_get_world_tension);
  lua_register(gLuaState, "activate_guardian_site", lua_activate_guardian_site);
  lua_register(gLuaState, "reveal_guardian_site", lua_reveal_guardian_site);
  lua_register(gLuaState, "assign_guardian_owner", lua_assign_guardian_owner);
  if (!w.mission.luaScriptInline.empty()) luaL_dostring(gLuaState, w.mission.luaScriptInline.c_str());
  else if (!w.mission.luaScriptFile.empty()) luaL_dofile(gLuaState, w.mission.luaScriptFile.c_str());
}
void run_lua_hook(World& w, const std::string& hook) {
  ensure_lua(w);
  if (!gLuaState || hook.empty()) return;
  gLuaWorld = &w;
  lua_getglobal(gLuaState, hook.c_str());
  if (lua_isfunction(gLuaState, -1)) {
    if (lua_pcall(gLuaState, 0, 0, 0) == LUA_OK) w.missionRuntime.luaHookLog.push_back(hook);
  } else lua_pop(gLuaState, 1);
  gLuaWorld = nullptr;
}
#else
void run_lua_hook(World& w, const std::string& hook) { if (!hook.empty()) w.missionRuntime.luaHookLog.push_back(std::string("lua-disabled:")+hook); }
#endif

constexpr uint16_t kRoleCount = static_cast<uint16_t>(UnitRole::Count);
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

struct NavRequest {
  uint64_t requestId{0};
  uint32_t tickIssued{0};
  uint32_t navVersion{0};
  int targetCell{0};
};

struct NavCompletion {
  uint64_t requestId{0};
  uint32_t navVersion{0};
  FlowField field{};
};

struct ChunkData {
  std::vector<uint32_t> unitIds;
  std::vector<uint32_t> buildingIds;
  std::vector<uint32_t> resourceIds;
  std::vector<int> fogTiles;
  std::vector<int> territoryTiles;
};

struct ChunkRuntime {
  int chunkWidth{16};
  int chunkHeight{16};
  int cols{0};
  int rows{0};
  std::vector<ChunkData> chunks;
} gChunks;

struct SpatialCell {
  std::vector<uint32_t> unitIds;
  std::vector<uint32_t> buildingIds;
};

struct SpatialGrid {
  float cellSize{6.0f};
  std::vector<SpatialCell> cells;
  std::vector<int> unitCell;
  std::vector<int> buildingCell;
  std::vector<int> queryCells;
  std::vector<uint32_t> queryUnits;
  std::vector<uint32_t> queryBuildings;
  std::unordered_map<uint32_t, size_t> unitIndexById;
  std::unordered_map<uint32_t, size_t> buildingIndexById;
};

SpatialGrid gSpatial;
TickProfile gLastTickProfile{};
SimulationStats gLastStats{};
uint64_t gNextNavRequestId{1};
std::vector<NavRequest> gPendingNavRequests;
std::vector<NavCompletion> gCompletedNavResults;

constexpr int32_t kInfCost = 1 << 29;
constexpr std::array<std::pair<int,int>, 8> kNeighborOrder{{{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}}};

constexpr float kWaterLevel = -0.18f;
constexpr float kShallowBand = 0.12f;

void queue_flow_field_request(World& w, int targetCell);
void ensure_chunk_layout(const World& w);
void process_nav_requests(World& w);
void apply_nav_results(World& w);
void rebuild_chunk_membership_impl(const World& w);
void load_guardian_defs(World& w);
void generate_guardian_sites(World& w);
void update_guardian_sites(World& w);
uint32_t spawn_guardian_unit(World& w, GuardianSiteInstance& site, const GuardianDefinition& def);

float unit_vision_radius(const Unit& u);
float building_vision_radius(BuildingType type);
float unit_detection_radius(const Unit& u);
bool unit_has_stealth(const Unit& u);
bool unit_is_recon(const Unit& u);
bool unit_is_air(UnitType t);


void emit_event(World& w, GameplayEventType type, uint16_t actor, uint16_t subject, uint32_t entityId, std::string text = {}) {
  gGameplayEvents.push_back({type, w.tick, actor, subject, entityId, std::move(text)});
}

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
    case BuildingType::Port: return "Port";
    case BuildingType::RadarTower: return "RadarTower";
    case BuildingType::MobileRadar: return "MobileRadar";
    case BuildingType::Airbase: return "Airbase";
    case BuildingType::MissileSilo: return "MissileSilo";
    case BuildingType::AABattery: return "AABattery";
    case BuildingType::AntiMissileDefense: return "AntiMissileDefense";
    case BuildingType::Count: break;
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
  if (v == "Port") return BuildingType::Port;
  if (v == "RadarTower") return BuildingType::RadarTower;
  if (v == "MobileRadar") return BuildingType::MobileRadar;
  if (v == "Airbase") return BuildingType::Airbase;
  if (v == "MissileSilo") return BuildingType::MissileSilo;
  if (v == "AABattery") return BuildingType::AABattery;
  if (v == "AntiMissileDefense") return BuildingType::AntiMissileDefense;
  return BuildingType::House;
}

const char* unit_name(UnitType t) {
  switch (t) {
    case UnitType::Worker: return "Worker";
    case UnitType::Infantry: return "Infantry";
    case UnitType::Archer: return "Archer";
    case UnitType::Cavalry: return "Cavalry";
    case UnitType::Siege: return "Siege";
    case UnitType::TransportShip: return "TransportShip";
    case UnitType::LightWarship: return "LightWarship";
    case UnitType::HeavyWarship: return "HeavyWarship";
    case UnitType::BombardShip: return "BombardShip";
    case UnitType::Fighter: return "Fighter";
    case UnitType::Interceptor: return "Interceptor";
    case UnitType::Bomber: return "Bomber";
    case UnitType::StrategicBomber: return "StrategicBomber";
    case UnitType::ReconDrone: return "ReconDrone";
    case UnitType::StrikeDrone: return "StrikeDrone";
    case UnitType::TacticalMissile: return "TacticalMissile";
    case UnitType::StrategicMissile: return "StrategicMissile";
    case UnitType::Count: break;
  }
  return "Infantry";
}

UnitType parse_unit(const std::string& v) {
  if (v == "Worker") return UnitType::Worker;
  if (v == "Archer") return UnitType::Archer;
  if (v == "Cavalry") return UnitType::Cavalry;
  if (v == "Siege") return UnitType::Siege;
  if (v == "TransportShip") return UnitType::TransportShip;
  if (v == "LightWarship") return UnitType::LightWarship;
  if (v == "HeavyWarship") return UnitType::HeavyWarship;
  if (v == "BombardShip") return UnitType::BombardShip;
  if (v == "Fighter") return UnitType::Fighter;
  if (v == "Interceptor") return UnitType::Interceptor;
  if (v == "Bomber") return UnitType::Bomber;
  if (v == "StrategicBomber") return UnitType::StrategicBomber;
  if (v == "ReconDrone") return UnitType::ReconDrone;
  if (v == "StrikeDrone") return UnitType::StrikeDrone;
  if (v == "TacticalMissile") return UnitType::TacticalMissile;
  if (v == "StrategicMissile") return UnitType::StrategicMissile;
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


ObjectiveCategory parse_objective_category(const std::string& v) {
  if (v == "secondary") return ObjectiveCategory::Secondary;
  if (v == "hidden" || v == "hidden_optional") return ObjectiveCategory::HiddenOptional;
  return ObjectiveCategory::Primary;
}

const char* objective_category_name(ObjectiveCategory c) {
  switch (c) {
    case ObjectiveCategory::Primary: return "primary";
    case ObjectiveCategory::Secondary: return "secondary";
    case ObjectiveCategory::HiddenOptional: return "hidden_optional";
  }
  return "primary";
}

const char* mission_status_name(MissionStatus s) {
  switch (s) {
    case MissionStatus::InBriefing: return "briefing";
    case MissionStatus::Running: return "running";
    case MissionStatus::Victory: return "victory";
    case MissionStatus::Defeat: return "defeat";
    case MissionStatus::PartialVictory: return "partial_victory";
  }
  return "running";
}

BiomeType parse_biome_type(const std::string& id) {
  if (id == "temperate_grassland") return BiomeType::TemperateGrassland;
  if (id == "plains_steppe") return BiomeType::Steppe;
  if (id == "forest") return BiomeType::Forest;
  if (id == "desert") return BiomeType::Desert;
  if (id == "mediterranean") return BiomeType::Mediterranean;
  if (id == "jungle") return BiomeType::Jungle;
  if (id == "tundra") return BiomeType::Tundra;
  if (id == "snow_arctic") return BiomeType::Arctic;
  if (id == "coast_littoral") return BiomeType::Coast;
  if (id == "wetlands_marsh") return BiomeType::Wetlands;
  if (id == "mountain_highlands") return BiomeType::Mountain;
  if (id == "snow_mountains" || id == "snow_capped_mountains") return BiomeType::SnowMountain;
  return BiomeType::TemperateGrassland;
}

const char* mineral_name(MineralType m) {
  switch (m) {
    case MineralType::Gold: return "gold";
    case MineralType::Iron: return "iron";
    case MineralType::Silver: return "silver";
    case MineralType::Copper: return "copper";
    case MineralType::Stone: return "stone";
    default: return "iron";
  }
}

const char* building_family_name(BuildingType t) {
  switch (t) {
    case BuildingType::CityCenter: return "CityCenter";
    case BuildingType::House: return "House";
    case BuildingType::Farm: return "Farm";
    case BuildingType::LumberCamp: return "LumberCamp";
    case BuildingType::Mine: return "Mine";
    case BuildingType::Market: return "Market";
    case BuildingType::Library: return "Granary";
    case BuildingType::Barracks: return "Barracks";
    case BuildingType::Wonder: return "Wonder";
    case BuildingType::Port: return "Port";
    default: return "House";
  }
}

struct BiomeDef {
  BiomeType type{BiomeType::TemperateGrassland};
  std::string id{"temperate_grassland"};
  std::string displayName{"Temperate Grassland"};
  std::array<float, 3> palette{0.32f, 0.62f, 0.28f};
  std::array<float, static_cast<size_t>(Resource::Count)> resourceWeight{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

struct ThemeDef {
  std::string id{"default"};
  std::unordered_map<std::string, std::string> familyToVariant;
};

struct CivilizationDef {
  std::string id{"default"};
  std::string displayName{"Default"};
  float economyBias{1.0f};
  float militaryBias{1.0f};
  float scienceBias{1.0f};
  float aggression{1.0f};
  float defense{1.0f};
  float diplomacyBias{1.0f};
  float logisticsBias{1.0f};
  float strategicBias{1.0f};
  float aiWorkerTargetMult{1.0f};
  float aiExpansionTiming{1.0f};
  float aiResearchPriority{1.0f};
  float aiReconPriority{1.0f};
  float aiNavalPriority{1.0f};
  float aiAirPriority{1.0f};
  float aiStrategicPriority{1.0f};
  std::array<float, static_cast<size_t>(UnitType::Count)> unitAttackMult{};
  std::array<float, static_cast<size_t>(UnitType::Count)> unitHpMult{};
  std::array<float, static_cast<size_t>(UnitType::Count)> unitCostMult{};
  std::array<float, static_cast<size_t>(UnitType::Count)> unitTrainTimeMult{};
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingCostMult{};
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingBuildTimeMult{};
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingHpMult{};
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingTrickleMult{};
  std::array<std::string, static_cast<size_t>(UnitType::Count)> uniqueUnitDefs{};
  std::array<std::string, static_cast<size_t>(BuildingType::Count)> uniqueBuildingDefs{};
  std::vector<std::string> missionTags;
};

std::vector<CivilizationDef> gCivilizations;
std::array<BiomeDef, static_cast<size_t>(BiomeType::Count)> gBiomes{};
std::unordered_map<std::string, ThemeDef> gThemes;

void init_civ_defaults(CivilizationDef& d) {
  d.unitAttackMult.fill(1.0f); d.unitHpMult.fill(1.0f); d.unitCostMult.fill(1.0f); d.unitTrainTimeMult.fill(1.0f);
  d.buildingCostMult.fill(1.0f); d.buildingBuildTimeMult.fill(1.0f); d.buildingHpMult.fill(1.0f); d.buildingTrickleMult.fill(1.0f);
}

template <typename TArr, typename TParser>
void parse_named_multiplier_map(const nlohmann::json& j, TArr& out, TParser parse) {
  if (!j.is_object()) return;
  for (auto it = j.begin(); it != j.end(); ++it) out[static_cast<size_t>(parse(it.key()))] = it.value().get<float>();
}

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
    init_civ_defaults(d);
    d.id = c.value("civilization_id", c.value("id", std::string("default")));
    d.displayName = c.value("display_name", c.value("displayName", d.id));
    d.economyBias = c.value("economyBias", 1.0f);
    d.militaryBias = c.value("militaryBias", 1.0f);
    d.scienceBias = c.value("scienceBias", 1.0f);
    d.aggression = c.value("aggression", 1.0f);
    d.defense = c.value("defense", 1.0f);
    d.diplomacyBias = c.value("diplomacyBias", 1.0f);
    d.logisticsBias = c.value("logisticsBias", 1.0f);
    d.strategicBias = c.value("strategicBias", 1.0f);
    if (c.contains("aiDoctrineModifiers")) {
      const auto& ai = c["aiDoctrineModifiers"];
      d.aiWorkerTargetMult = ai.value("workerTargetMult", 1.0f);
      d.aiExpansionTiming = ai.value("expansionTiming", 1.0f);
      d.aiResearchPriority = ai.value("researchPriority", 1.0f);
      d.aiReconPriority = ai.value("reconPriority", 1.0f);
      d.aiNavalPriority = ai.value("navalPriority", 1.0f);
      d.aiAirPriority = ai.value("airPriority", 1.0f);
      d.aiStrategicPriority = ai.value("strategicPriority", 1.0f);
    }
    if (c.contains("unitBonuses")) {
      const auto& ub = c["unitBonuses"];
      if (ub.contains("attackMult")) parse_named_multiplier_map(ub["attackMult"], d.unitAttackMult, parse_unit);
      if (ub.contains("hpMult")) parse_named_multiplier_map(ub["hpMult"], d.unitHpMult, parse_unit);
      if (ub.contains("costMult")) parse_named_multiplier_map(ub["costMult"], d.unitCostMult, parse_unit);
      if (ub.contains("trainTimeMult")) parse_named_multiplier_map(ub["trainTimeMult"], d.unitTrainTimeMult, parse_unit);
    }
    if (c.contains("buildingBonuses")) {
      const auto& bb = c["buildingBonuses"];
      if (bb.contains("costMult")) parse_named_multiplier_map(bb["costMult"], d.buildingCostMult, parse_building);
      if (bb.contains("buildTimeMult")) parse_named_multiplier_map(bb["buildTimeMult"], d.buildingBuildTimeMult, parse_building);
      if (bb.contains("hpMult")) parse_named_multiplier_map(bb["hpMult"], d.buildingHpMult, parse_building);
      if (bb.contains("trickleMult")) parse_named_multiplier_map(bb["trickleMult"], d.buildingTrickleMult, parse_building);
    }
    if (c.contains("uniqueUnits") && c["uniqueUnits"].is_object()) for (auto it=c["uniqueUnits"].begin(); it!=c["uniqueUnits"].end(); ++it) d.uniqueUnitDefs[static_cast<size_t>(parse_unit(it.key()))]=it.value().get<std::string>();
    if (c.contains("uniqueBuildings") && c["uniqueBuildings"].is_object()) for (auto it=c["uniqueBuildings"].begin(); it!=c["uniqueBuildings"].end(); ++it) d.uniqueBuildingDefs[static_cast<size_t>(parse_building(it.key()))]=it.value().get<std::string>();
    if (c.contains("missionTags") && c["missionTags"].is_array()) d.missionTags = c["missionTags"].get<std::vector<std::string>>();
    gCivilizations.push_back(d);
  }
  if (gCivilizations.empty()) gCivilizations.push_back({});
}

CivilizationRuntime civilization_runtime_for(const std::string& id) {
  load_civilizations_once();
  for (const auto& c : gCivilizations) if (c.id == id) {
    CivilizationRuntime r{};
    r.id = c.id; r.displayName = c.displayName; r.economyBias = c.economyBias; r.militaryBias = c.militaryBias; r.scienceBias = c.scienceBias; r.aggression = c.aggression; r.defense = c.defense;
    r.diplomacyBias = c.diplomacyBias; r.logisticsBias = c.logisticsBias; r.strategicBias = c.strategicBias;
    r.aiWorkerTargetMult = c.aiWorkerTargetMult; r.aiExpansionTiming = c.aiExpansionTiming; r.aiResearchPriority = c.aiResearchPriority; r.aiReconPriority = c.aiReconPriority; r.aiNavalPriority = c.aiNavalPriority; r.aiAirPriority = c.aiAirPriority; r.aiStrategicPriority = c.aiStrategicPriority;
    r.unitAttackMult = c.unitAttackMult; r.unitHpMult = c.unitHpMult; r.unitCostMult = c.unitCostMult; r.unitTrainTimeMult = c.unitTrainTimeMult;
    r.buildingCostMult = c.buildingCostMult; r.buildingBuildTimeMult = c.buildingBuildTimeMult; r.buildingHpMult = c.buildingHpMult; r.buildingTrickleMult = c.buildingTrickleMult;
    r.uniqueUnitDefs = c.uniqueUnitDefs; r.uniqueBuildingDefs = c.uniqueBuildingDefs; r.missionTags = c.missionTags;
    return r;
  }
  CivilizationRuntime d{};
  d.unitAttackMult.fill(1.0f); d.unitHpMult.fill(1.0f); d.unitCostMult.fill(1.0f); d.unitTrainTimeMult.fill(1.0f);
  d.buildingCostMult.fill(1.0f); d.buildingBuildTimeMult.fill(1.0f); d.buildingHpMult.fill(1.0f); d.buildingTrickleMult.fill(1.0f);
  return d;
}

void load_biomes_once() {
  if (!gBiomes[0].id.empty()) return;
  std::ifstream f("content/biomes.json");
  if (!f.good()) return;
  nlohmann::json j; f >> j;
  if (!j.contains("biomes") || !j["biomes"].is_array()) return;
  for (const auto& b : j["biomes"]) {
    std::string id = b.value("id", std::string("temperate_grassland"));
    BiomeType bt = parse_biome_type(id);
    auto& out = gBiomes[static_cast<size_t>(bt)];
    out.type = bt;
    out.id = id;
    out.displayName = b.value("display_name", id);
    if (b.contains("color_palette_hint") && b["color_palette_hint"].is_array() && b["color_palette_hint"].size() >= 3) {
      out.palette = {b["color_palette_hint"][0].get<float>(), b["color_palette_hint"][1].get<float>(), b["color_palette_hint"][2].get<float>()};
    }
  }
}

void load_civilization_themes_once() {
  if (!gThemes.empty()) return;
  std::ifstream f("content/civilization_themes.json");
  if (!f.good()) return;
  nlohmann::json j; f >> j;
  if (!j.contains("themes") || !j["themes"].is_array()) return;
  for (const auto& t : j["themes"]) {
    ThemeDef theme{};
    theme.id = t.value("id", std::string("default"));
    if (t.contains("building_family_mappings") && t["building_family_mappings"].is_object()) {
      for (auto it = t["building_family_mappings"].begin(); it != t["building_family_mappings"].end(); ++it) {
        theme.familyToVariant[it.key()] = it.value().get<std::string>();
      }
    }
    gThemes[theme.id] = std::move(theme);
  }
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

  gBuildDefs[bidx(BuildingType::Port)].size = {3.2f, 3.2f};
  gBuildDefs[bidx(BuildingType::Port)].buildTime = 22.0f;
  gBuildDefs[bidx(BuildingType::Port)].cost[ridx(Resource::Wood)] = 140;
  gBuildDefs[bidx(BuildingType::Port)].cost[ridx(Resource::Metal)] = 80;

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
  gUnitDefs[uidx(UnitType::Infantry)].vsRoleMultiplierPermille = {1000, 1300, 900, 900, 1000, 1000, 1000, 1000};

  gUnitDefs[uidx(UnitType::Archer)].trainTime = 13.0f;
  gUnitDefs[uidx(UnitType::Archer)].cost[ridx(Resource::Food)] = 65;
  gUnitDefs[uidx(UnitType::Archer)].cost[ridx(Resource::Wood)] = 35;
  gUnitDefs[uidx(UnitType::Archer)].popCost = 1;
  gUnitDefs[uidx(UnitType::Archer)].role = UnitRole::Ranged;
  gUnitDefs[uidx(UnitType::Archer)].attackType = AttackType::Ranged;
  gUnitDefs[uidx(UnitType::Archer)].preferredTargetRole = UnitRole::Cavalry;
  gUnitDefs[uidx(UnitType::Archer)].vsRoleMultiplierPermille = {1000, 900, 1300, 1000, 1000, 900, 1000, 1000};
  gUnitDefs[uidx(UnitType::Archer)].attackCooldownTicks = 16;

  gUnitDefs[uidx(UnitType::Cavalry)].trainTime = 16.0f;
  gUnitDefs[uidx(UnitType::Cavalry)].cost[ridx(Resource::Food)] = 95;
  gUnitDefs[uidx(UnitType::Cavalry)].cost[ridx(Resource::Metal)] = 45;
  gUnitDefs[uidx(UnitType::Cavalry)].popCost = 2;
  gUnitDefs[uidx(UnitType::Cavalry)].role = UnitRole::Cavalry;
  gUnitDefs[uidx(UnitType::Cavalry)].attackType = AttackType::Melee;
  gUnitDefs[uidx(UnitType::Cavalry)].preferredTargetRole = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::Cavalry)].vsRoleMultiplierPermille = {1000, 1000, 900, 1300, 1100, 900, 1000, 1000};

  gUnitDefs[uidx(UnitType::Siege)].trainTime = 18.0f;
  gUnitDefs[uidx(UnitType::Siege)].cost[ridx(Resource::Wood)] = 90;
  gUnitDefs[uidx(UnitType::Siege)].cost[ridx(Resource::Metal)] = 100;
  gUnitDefs[uidx(UnitType::Siege)].popCost = 2;
  gUnitDefs[uidx(UnitType::Siege)].role = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::Siege)].attackType = AttackType::Ranged;
  gUnitDefs[uidx(UnitType::Siege)].preferredTargetRole = UnitRole::Building;
  gUnitDefs[uidx(UnitType::Siege)].vsRoleMultiplierPermille = {900, 900, 900, 1000, 900, 1800, 900, 900};
  gUnitDefs[uidx(UnitType::Siege)].attackCooldownTicks = 22;

  gUnitDefs[uidx(UnitType::TransportShip)].trainTime = 16.0f;
  gUnitDefs[uidx(UnitType::TransportShip)].cost[ridx(Resource::Wood)] = 120;
  gUnitDefs[uidx(UnitType::TransportShip)].cost[ridx(Resource::Metal)] = 40;
  gUnitDefs[uidx(UnitType::TransportShip)].popCost = 2;
  gUnitDefs[uidx(UnitType::TransportShip)].role = UnitRole::Transport;

  gUnitDefs[uidx(UnitType::LightWarship)].trainTime = 18.0f;
  gUnitDefs[uidx(UnitType::LightWarship)].cost[ridx(Resource::Wood)] = 100;
  gUnitDefs[uidx(UnitType::LightWarship)].cost[ridx(Resource::Metal)] = 70;
  gUnitDefs[uidx(UnitType::LightWarship)].popCost = 2;
  gUnitDefs[uidx(UnitType::LightWarship)].role = UnitRole::Naval;

  gUnitDefs[uidx(UnitType::HeavyWarship)].trainTime = 22.0f;
  gUnitDefs[uidx(UnitType::HeavyWarship)].cost[ridx(Resource::Wood)] = 120;
  gUnitDefs[uidx(UnitType::HeavyWarship)].cost[ridx(Resource::Metal)] = 130;
  gUnitDefs[uidx(UnitType::HeavyWarship)].popCost = 3;
  gUnitDefs[uidx(UnitType::HeavyWarship)].role = UnitRole::Naval;

  gUnitDefs[uidx(UnitType::BombardShip)].trainTime = 24.0f;
  gUnitDefs[uidx(UnitType::BombardShip)].cost[ridx(Resource::Wood)] = 140;
  gUnitDefs[uidx(UnitType::BombardShip)].cost[ridx(Resource::Metal)] = 110;
  gUnitDefs[uidx(UnitType::BombardShip)].popCost = 3;
  gUnitDefs[uidx(UnitType::BombardShip)].role = UnitRole::Naval;

  gUnitDefs[uidx(UnitType::Fighter)].trainTime = 18.0f; gUnitDefs[uidx(UnitType::Fighter)].cost[ridx(Resource::Metal)] = 120; gUnitDefs[uidx(UnitType::Fighter)].cost[ridx(Resource::Oil)] = 20; gUnitDefs[uidx(UnitType::Fighter)].role = UnitRole::Ranged;
  gUnitDefs[uidx(UnitType::Interceptor)] = gUnitDefs[uidx(UnitType::Fighter)]; gUnitDefs[uidx(UnitType::Interceptor)].cost[ridx(Resource::Metal)] = 110;
  gUnitDefs[uidx(UnitType::Bomber)].trainTime = 22.0f; gUnitDefs[uidx(UnitType::Bomber)].cost[ridx(Resource::Metal)] = 150; gUnitDefs[uidx(UnitType::Bomber)].cost[ridx(Resource::Oil)] = 40; gUnitDefs[uidx(UnitType::Bomber)].role = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::StrategicBomber)] = gUnitDefs[uidx(UnitType::Bomber)]; gUnitDefs[uidx(UnitType::StrategicBomber)].trainTime = 28.0f; gUnitDefs[uidx(UnitType::StrategicBomber)].cost[ridx(Resource::Metal)] = 220;
  gUnitDefs[uidx(UnitType::ReconDrone)].trainTime = 14.0f; gUnitDefs[uidx(UnitType::ReconDrone)].cost[ridx(Resource::Metal)] = 80; gUnitDefs[uidx(UnitType::ReconDrone)].cost[ridx(Resource::Knowledge)] = 40; gUnitDefs[uidx(UnitType::ReconDrone)].role = UnitRole::Ranged;
  gUnitDefs[uidx(UnitType::StrikeDrone)] = gUnitDefs[uidx(UnitType::ReconDrone)]; gUnitDefs[uidx(UnitType::StrikeDrone)].cost[ridx(Resource::Metal)] = 120;
  gUnitDefs[uidx(UnitType::TacticalMissile)].trainTime = 26.0f; gUnitDefs[uidx(UnitType::TacticalMissile)].cost[ridx(Resource::Metal)] = 180; gUnitDefs[uidx(UnitType::TacticalMissile)].cost[ridx(Resource::Oil)] = 80; gUnitDefs[uidx(UnitType::TacticalMissile)].role = UnitRole::Siege;
  gUnitDefs[uidx(UnitType::StrategicMissile)] = gUnitDefs[uidx(UnitType::TacticalMissile)]; gUnitDefs[uidx(UnitType::StrategicMissile)].trainTime = 36.0f; gUnitDefs[uidx(UnitType::StrategicMissile)].cost[ridx(Resource::Metal)] = 260;

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
      else if (id == "Port") t = BuildingType::Port;
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
      else if (id == "TransportShip") t = UnitType::TransportShip;
      else if (id == "LightWarship") t = UnitType::LightWarship;
      else if (id == "HeavyWarship") t = UnitType::HeavyWarship;
      else if (id == "BombardShip") t = UnitType::BombardShip;
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

  std::ifstream g("content/mythic_guardians.json");
  if (g.good()) {
    nlohmann::json gj; g >> gj;
    if (gj.contains("guardians") && gj["guardians"].is_array()) {
      // loaded per world in initialize/load path; this parse validates content shape once.
      for (const auto& gd : gj["guardians"]) {
        (void)gd.value("guardian_id", std::string{});
      }
    }
  }
  gDefsLoaded = true;
}

void load_guardian_defs(World& w) {
  w.guardianDefinitions.clear();
  std::ifstream g("content/mythic_guardians.json");
  if (!g.good()) return;
  nlohmann::json j; g >> j;
  if (!j.contains("guardians") || !j["guardians"].is_array()) return;
  for (const auto& gd : j["guardians"]) {
    GuardianDefinition d{};
    d.guardianId = gd.value("guardian_id", std::string{});
    if (d.guardianId.empty()) continue;
    d.displayName = gd.value("display_name", d.guardianId);
    d.biomeRequirement = static_cast<uint8_t>(parse_biome_type(gd.value("biome_requirement", std::string("snow_mountains"))));
    d.siteType = parse_guardian_site_type(gd.value("site_type", std::string("yeti_lair")));
    d.spawnMode = parse_guardian_spawn_mode(gd.value("spawn_mode", std::string("on_discovery")));
    d.maxPerMap = gd.value("max_per_map", 1u);
    d.unique = gd.value("unique", true);
    d.discoveryMode = parse_guardian_discovery_mode(gd.value("discovery_mode", std::string("proximity")));
    d.behaviorMode = parse_guardian_behavior_mode(gd.value("behavior_mode", std::string("dormant_until_discovery")));
    d.joinMode = parse_guardian_join_mode(gd.value("join_mode", std::string("discoverer_control")));
    d.associatedUnitDefinitionId = gd.value("associated_unit_definition", std::string{});
    d.rewardHook = gd.value("reward_hook", std::string{});
    d.effectHook = gd.value("effect_hook", std::string{});
    d.scenarioOnly = gd.value("scenario_only", false);
    d.procedural = gd.value("procedural", true);
    d.rarityPermille = gd.value("rarity_permille", 8);
    d.minSpacingCells = gd.value("min_spacing_cells", 20);
    d.discoveryRadius = gd.value("discovery_radius", 5.0f);
    if (gd.contains("unit") && gd["unit"].is_object()) {
      const auto& u = gd["unit"];
      d.unitHp = u.value("hp", d.unitHp);
      d.unitAttack = u.value("attack", d.unitAttack);
      d.unitRange = u.value("range", d.unitRange);
      d.unitSpeed = u.value("speed", d.unitSpeed);
    }
    w.guardianDefinitions.push_back(std::move(d));
  }
  std::sort(w.guardianDefinitions.begin(), w.guardianDefinitions.end(), [](const GuardianDefinition& a, const GuardianDefinition& b){ return a.guardianId < b.guardianId; });
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



TerrainClass terrain_class_at(const World& w, int idx) {
  if (idx < 0 || idx >= (int)w.terrainClass.size()) return TerrainClass::Land;
  return static_cast<TerrainClass>(w.terrainClass[idx]);
}

bool is_water_class(TerrainClass t) { return t == TerrainClass::ShallowWater || t == TerrainClass::DeepWater; }

bool unit_is_naval(UnitType t) {
  return t == UnitType::TransportShip || t == UnitType::LightWarship || t == UnitType::HeavyWarship || t == UnitType::BombardShip;
}

bool unit_can_embark(UnitType t) {
  return t == UnitType::Infantry || t == UnitType::Archer || t == UnitType::Cavalry || t == UnitType::Siege || t == UnitType::Worker;
}

bool unit_cell_valid(const World& w, const Unit& u, int cell) {
  TerrainClass tc = terrain_class_at(w, cell);
  if (u.embarked) return false;
  if (unit_is_air(u.type)) return true;
  if (unit_is_naval(u.type)) return is_water_class(tc);
  return !is_water_class(tc);
}

bool has_adjacent_coast(const World& w, glm::vec2 p) {
  int x = std::clamp((int)p.x, 0, w.width - 1);
  int y = std::clamp((int)p.y, 0, w.height - 1);
  for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
    int nx = std::clamp(x + ox, 0, w.width - 1);
    int ny = std::clamp(y + oy, 0, w.height - 1);
    TerrainClass tc = terrain_class_at(w, ny * w.width + nx);
    if (tc == TerrainClass::ShallowWater) return true;
  }
  return false;
}

float sample_noise_2d(uint32_t seed, int x, int y, float scale) {
  const float fx = static_cast<float>(x) * scale;
  const float fy = static_cast<float>(y) * scale;
  const float v = std::sin(fx * 12.9898f + fy * 78.233f + seed * 0.0031f) * 43758.5453f;
  return v - std::floor(v);
}

void generate_macro_landmass(World& w) {
  const int cells = w.width * w.height;
  w.landmassIdByCell.assign(static_cast<size_t>(cells), -1);
  for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
    const int i = y * w.width + x;
    const float nx = (float)x / std::max(1, w.width - 1) - 0.5f;
    const float ny = (float)y / std::max(1, w.height - 1) - 0.5f;
    const float radial = std::sqrt(nx * nx + ny * ny);
    const float n1 = sample_noise_2d(w.seed + 11, x, y, 0.028f) * 2.0f - 1.0f;
    const float n2 = sample_noise_2d(w.seed + 29, x, y, 0.061f) * 2.0f - 1.0f;
    const float n3 = sample_noise_2d(w.seed + 53, x, y, 0.11f) * 2.0f - 1.0f;
    float base = n1 * 0.60f + n2 * 0.28f + n3 * 0.12f;
    float threshold = 0.0f;
    if (w.worldPreset == WorldPreset::Pangaea) {
      base += (0.62f - radial * 1.28f) + std::max(0.0f, 0.18f - std::abs(nx) * 0.35f);
      threshold = 0.04f;
    } else if (w.worldPreset == WorldPreset::Continents) {
      base += (0.30f - radial * 0.58f) + std::sin((x + w.seed * 0.03f) * 0.05f) * 0.16f;
      threshold = 0.10f;
    } else if (w.worldPreset == WorldPreset::Archipelago) {
      base += (0.18f - radial * 0.22f) + std::sin((x + y) * 0.09f) * 0.12f;
      threshold = 0.24f;
    } else if (w.worldPreset == WorldPreset::InlandSea) {
      const float sea = 0.32f - std::sqrt(nx * nx + ny * ny) * 1.4f;
      base += 0.62f - std::max(0.0f, sea);
      threshold = 0.22f;
    } else if (w.worldPreset == WorldPreset::MountainWorld) {
      base += (0.40f - radial * 0.72f);
      threshold = 0.08f;
    }
    w.heightmap[i] = base - threshold;
  }
}

void apply_tectonics(World& w) {
  const float dir = 0.55f + sample_noise_2d(w.seed + 700, 3, 7, 1.0f) * 0.9f;
  for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
    const int i = y * w.width + x;
    const float seam = std::sin((x * std::cos(dir) + y * std::sin(dir)) * 0.08f + w.seed * 0.003f);
    const float ridge = std::pow(std::max(0.0f, seam), 3.0f);
    const float branch = std::pow(std::max(0.0f, std::sin((x - y) * 0.06f + w.seed * 0.002f)), 2.0f);
    float uplift = ridge * 0.48f + branch * 0.22f;
    if (w.worldPreset == WorldPreset::MountainWorld) uplift *= 1.8f;
    if (w.heightmap[i] > kWaterLevel + 0.03f) w.heightmap[i] += uplift;
    else w.heightmap[i] -= ridge * 0.08f;
  }
}

void classify_coast_and_landmass(World& w) {
  const int cells = w.width * w.height;
  w.coastClassMap.assign(static_cast<size_t>(cells), 0);
  w.landmassIdByCell.assign(static_cast<size_t>(cells), -1);
  std::vector<uint8_t> seen(static_cast<size_t>(cells), 0);
  int nextLand = 1;
  for (int i = 0; i < cells; ++i) {
    if (w.terrainClass[i] != static_cast<uint8_t>(TerrainClass::Land) || seen[i]) continue;
    std::queue<int> q;
    q.push(i); seen[i] = 1;
    while (!q.empty()) {
      int c = q.front(); q.pop();
      w.landmassIdByCell[(size_t)c] = nextLand;
      int cx = c % w.width;
      int cy = c / w.width;
      bool adjShallow = false;
      bool adjDeep = false;
      for (auto [dx,dy] : kNeighborOrder) {
        int nx = cx + dx, ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        int ni = ny * w.width + nx;
        if (w.terrainClass[ni] == static_cast<uint8_t>(TerrainClass::Land) && !seen[ni]) { seen[ni] = 1; q.push(ni); }
        if (w.terrainClass[ni] == static_cast<uint8_t>(TerrainClass::ShallowWater)) adjShallow = true;
        if (w.terrainClass[ni] == static_cast<uint8_t>(TerrainClass::DeepWater)) adjDeep = true;
      }
      w.coastClassMap[(size_t)c] = adjShallow ? (adjDeep ? 2 : 1) : 0;
    }
    ++nextLand;
  }
}

void assign_biomes(World& w) {
  load_biomes_once();
  w.temperatureMap.assign(static_cast<size_t>(w.width) * static_cast<size_t>(w.height), 0.0f);
  w.moistureMap.assign(static_cast<size_t>(w.width) * static_cast<size_t>(w.height), 0.0f);
  w.biomeMap.assign(static_cast<size_t>(w.width) * static_cast<size_t>(w.height), static_cast<uint8_t>(BiomeType::TemperateGrassland));
  for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
    const int i = y * w.width + x;
    const float h = w.heightmap[i];
    const float rugged = sample_noise_2d(w.seed + 401, x, y, 0.21f);
    const float coast = w.coastClassMap.empty() ? 0.0f : (w.coastClassMap[i] > 0 ? 1.0f : 0.0f);
    float rainShadow = 0.0f;
    for (int ox = 1; ox <= 6; ++ox) {
      int nx = std::clamp(x - ox, 0, w.width - 1);
      int ni = y * w.width + nx;
      rainShadow += std::max(0.0f, w.heightmap[ni] - 0.48f) * 0.08f;
    }
    float moisture = std::clamp(w.fertility[i] * 0.55f + coast * 0.18f + sample_noise_2d(w.seed + 17, x, y, 0.13f) * 0.27f - rainShadow, 0.0f, 1.0f);
    const float lat = std::abs((float)y / std::max(1, w.height - 1) - 0.5f) * 2.0f;
    const float temp = std::clamp((1.0f - lat) * 0.84f + sample_noise_2d(w.seed + 51, x, y, 0.09f) * 0.16f - std::max(0.0f, h) * 0.35f, 0.0f, 1.0f);
    w.temperatureMap[i] = temp;
    w.moistureMap[i] = moisture;
    BiomeType b = BiomeType::TemperateGrassland;
    TerrainClass tc = static_cast<TerrainClass>(w.terrainClass[i]);
    if (tc != TerrainClass::Land) b = BiomeType::Coast;
    else if (h > 0.72f || (h > 0.64f && rugged > 0.6f)) {
      const bool snowCap = h > 0.86f || (h > 0.78f && temp < 0.36f);
      b = snowCap ? BiomeType::SnowMountain : BiomeType::Mountain;
    } else if (temp < 0.18f) b = BiomeType::Arctic;
    else if (temp < 0.28f) b = BiomeType::Tundra;
    else if (temp > 0.75f && moisture < 0.25f) b = BiomeType::Desert;
    else if (temp > 0.68f && moisture > 0.72f) b = BiomeType::Jungle;
    else if (moisture > 0.78f) b = BiomeType::Wetlands;
    else if (moisture > 0.62f) b = BiomeType::Forest;
    else if (temp > 0.56f && moisture > 0.35f && moisture < 0.6f) b = BiomeType::Mediterranean;
    else if (moisture < 0.32f) b = BiomeType::Steppe;
    w.biomeMap[i] = static_cast<uint8_t>(b);
  }
}

void rebuild_mountain_regions(World& w) {
  const int cells = w.width * w.height;
  w.mountainRegionByCell.assign(static_cast<size_t>(cells), -1);
  w.mountainRegions.clear();
  std::vector<uint8_t> seen(static_cast<size_t>(cells), 0);
  uint32_t nextRegionId = 1;
  for (int i = 0; i < cells; ++i) {
    if (seen[i]) continue;
    BiomeType b = biome_at(w, i);
    if (!(b == BiomeType::Mountain || b == BiomeType::SnowMountain)) continue;
    std::queue<int> q;
    q.push(i);
    seen[i] = 1;
    MountainRegion region{};
    region.id = nextRegionId++;
    region.minX = region.maxX = i % w.width;
    region.minY = region.maxY = i / w.width;
    float bestH = -1000.0f;
    int weightedX = 0;
    int weightedY = 0;
    while (!q.empty()) {
      const int c = q.front(); q.pop();
      const int cx = c % w.width;
      const int cy = c / w.width;
      w.mountainRegionByCell[static_cast<size_t>(c)] = static_cast<int32_t>(region.id);
      region.cellCount++;
      weightedX += cx;
      weightedY += cy;
      region.minX = std::min(region.minX, cx);
      region.maxX = std::max(region.maxX, cx);
      region.minY = std::min(region.minY, cy);
      region.maxY = std::max(region.maxY, cy);
      if (w.heightmap[static_cast<size_t>(c)] > bestH) { bestH = w.heightmap[static_cast<size_t>(c)]; region.peakCell = c; }
      for (auto [dx, dy] : kNeighborOrder) {
        const int nx = cx + dx;
        const int ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        const int ni = ny * w.width + nx;
        if (seen[static_cast<size_t>(ni)]) continue;
        BiomeType nb = biome_at(w, ni);
        if (!(nb == BiomeType::Mountain || nb == BiomeType::SnowMountain)) continue;
        seen[static_cast<size_t>(ni)] = 1;
        q.push(ni);
      }
    }
    if (region.cellCount > 0) {
      region.centerCell = (weightedY / static_cast<int>(region.cellCount)) * w.width + (weightedX / static_cast<int>(region.cellCount));
      w.mountainRegions.push_back(region);
    }
  }
}

void rebuild_mountain_deposits(World& w) {
  w.surfaceDeposits.clear();
  w.deepDeposits.clear();
  w.undergroundNodes.clear();
  w.undergroundEdges.clear();
  uint32_t sid = 1;
  uint32_t did = 1;
  uint32_t nid = 1;
  uint32_t eid = 1;
  const std::array<MineralType, 5> minerals{MineralType::Gold, MineralType::Iron, MineralType::Silver, MineralType::Copper, MineralType::Stone};
  for (const auto& region : w.mountainRegions) {
    if (region.cellCount < 6) continue;
    const int surfaceCount = std::max(1, static_cast<int>(region.cellCount / 42));
    const int deepCount = std::max(1, static_cast<int>(region.cellCount / 28));
    for (int i = 0; i < surfaceCount; ++i) {
      int cx = std::clamp(region.minX + ((i + 1) * 7 + static_cast<int>(region.id)) % std::max(1, region.maxX - region.minX + 1), 0, w.width - 1);
      int cy = std::clamp(region.minY + ((i + 2) * 5 + static_cast<int>(region.id) * 3) % std::max(1, region.maxY - region.minY + 1), 0, w.height - 1);
      int cell = cy * w.width + cx;
      if (w.mountainRegionByCell[static_cast<size_t>(cell)] != static_cast<int32_t>(region.id)) continue;
      SurfaceDeposit sd{};
      sd.id = sid++;
      sd.regionId = region.id;
      sd.mineral = minerals[(i + static_cast<int>(region.id)) % minerals.size()];
      sd.cell = cell;
      sd.remaining = 800.0f + 120.0f * ((i + static_cast<int>(region.id)) % 5);
      w.surfaceDeposits.push_back(sd);
      w.resourceNodes.push_back({static_cast<uint32_t>(w.resourceNodes.size()+1), ResourceNodeType::Ore, {cx + 0.5f, cy + 0.5f}, sd.remaining, UINT16_MAX});
    }

    UndergroundNode shaft{};
    shaft.id = nid++;
    shaft.regionId = region.id;
    shaft.type = UndergroundNodeType::Shaft;
    shaft.cell = region.centerCell;
    w.undergroundNodes.push_back(shaft);

    UndergroundNode depot{};
    depot.id = nid++;
    depot.regionId = region.id;
    depot.type = UndergroundNodeType::Depot;
    depot.cell = region.centerCell;
    w.undergroundNodes.push_back(depot);

    UndergroundEdge root{};
    root.id = eid++;
    root.regionId = region.id;
    root.a = shaft.id;
    root.b = depot.id;
    w.undergroundEdges.push_back(root);

    uint32_t prevNode = depot.id;
    for (int i = 0; i < deepCount; ++i) {
      int cell = region.peakCell;
      if (cell < 0) cell = region.centerCell;
      int cx = (cell % w.width) + (i % 2 ? 1 : -1);
      int cy = (cell / w.width) + (i % 3 ? 1 : -1);
      cx = std::clamp(cx, region.minX, region.maxX);
      cy = std::clamp(cy, region.minY, region.maxY);
      cell = cy * w.width + cx;
      if (w.mountainRegionByCell[static_cast<size_t>(cell)] != static_cast<int32_t>(region.id)) cell = region.centerCell;

      UndergroundNode dn{};
      dn.id = nid++;
      dn.regionId = region.id;
      dn.type = UndergroundNodeType::Deposit;
      dn.cell = cell;
      w.undergroundNodes.push_back(dn);

      DeepDeposit dd{};
      dd.id = did++;
      dd.regionId = region.id;
      dd.nodeId = dn.id;
      dd.mineral = minerals[(i + 2 + static_cast<int>(region.id)) % 4];
      dd.cell = cell;
      dd.richness = 1.7f + 0.2f * (i % 3);
      dd.remaining = 1800.0f + 300.0f * (i % 4);
      w.deepDeposits.push_back(dd);

      UndergroundEdge e{};
      e.id = eid++;
      e.regionId = region.id;
      e.a = prevNode;
      e.b = dn.id;
      w.undergroundEdges.push_back(e);
      prevNode = dn.id;
    }
  }
}

void generate_hydrology(World& w) {
  const int cells = w.width * w.height;
  w.riverMap.assign(static_cast<size_t>(cells), 0);
  w.lakeMap.assign(static_cast<size_t>(cells), 0);
  int riverCount = 0;
  int lakeCount = 0;
  for (int i = 0; i < cells; ++i) {
    if (w.terrainClass[i] != static_cast<uint8_t>(TerrainClass::Land)) continue;
    if (w.heightmap[i] < 0.60f) continue;
    if (sample_noise_2d(w.seed + 821, i % w.width, i / w.width, 0.2f) < 0.83f) continue;
    int current = i;
    bool reachedWater = false;
    for (int step = 0; step < 90; ++step) {
      if (current < 0 || current >= cells) break;
      w.riverMap[(size_t)current] = 1;
      int cx = current % w.width;
      int cy = current / w.width;
      int best = current;
      float bestH = w.heightmap[(size_t)current];
      for (auto [dx,dy] : kNeighborOrder) {
        int nx = cx + dx, ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        int ni = ny * w.width + nx;
        if (w.heightmap[(size_t)ni] < bestH) { bestH = w.heightmap[(size_t)ni]; best = ni; }
      }
      if (best == current) {
        if (w.terrainClass[(size_t)current] == static_cast<uint8_t>(TerrainClass::Land)) {
          w.lakeMap[(size_t)current] = 1;
          ++lakeCount;
        }
        break;
      }
      current = best;
      if (w.terrainClass[(size_t)current] != static_cast<uint8_t>(TerrainClass::Land)) { reachedWater = true; break; }
    }
    if (reachedWater) ++riverCount;
  }
  w.riverCount = static_cast<uint32_t>(riverCount);
  w.lakeCount = static_cast<uint32_t>(lakeCount);
}

void build_resource_geography(World& w) {
  w.resourceWeightMap.assign(static_cast<size_t>(w.width) * static_cast<size_t>(w.height), 0.0f);
  for (int y = 0; y < w.height; y += 8) for (int x = 0; x < w.width; x += 8) {
    int i = y * w.width + x;
    if (w.terrainClass[(size_t)i] != static_cast<uint8_t>(TerrainClass::Land)) continue;
    const BiomeType b = biome_at(w, i);
    float weight = 0.2f;
    if (b == BiomeType::Mountain || b == BiomeType::SnowMountain) weight += 0.9f;
    if (b == BiomeType::Forest || b == BiomeType::Jungle) weight += 0.55f;
    if (b == BiomeType::TemperateGrassland || b == BiomeType::Mediterranean || b == BiomeType::Steppe) weight += 0.35f;
    if (w.riverMap.size() == w.heightmap.size() && w.riverMap[(size_t)i]) weight += 0.65f;
    if (w.lakeMap.size() == w.heightmap.size() && w.lakeMap[(size_t)i]) weight += 0.5f;
    if (w.coastClassMap.size() == w.heightmap.size() && w.coastClassMap[(size_t)i] > 0) weight += 0.3f;
    w.resourceWeightMap[(size_t)i] = weight;
  }
}

void build_start_candidates(World& w) {
  w.startCandidates.clear();
  for (int y = 6; y < w.height - 6; y += 6) for (int x = 6; x < w.width - 6; x += 6) {
    int i = y * w.width + x;
    if (w.terrainClass[(size_t)i] != static_cast<uint8_t>(TerrainClass::Land)) continue;
    const BiomeType b = biome_at(w, i);
    if (b == BiomeType::Mountain || b == BiomeType::SnowMountain || b == BiomeType::Arctic) continue;
    float score = 0.0f;
    score += std::clamp(w.fertility[(size_t)i], 0.0f, 1.0f) * 1.3f;
    if (!w.resourceWeightMap.empty()) score += std::min(1.4f, w.resourceWeightMap[(size_t)i]);
    if (!w.riverMap.empty() && w.riverMap[(size_t)i]) score += 0.8f;
    if (!w.coastClassMap.empty() && w.coastClassMap[(size_t)i] > 0) score += 0.45f;
    score += (w.heightmap[(size_t)i] > 0.42f ? 0.4f : 0.0f);
    uint8_t bias = 0;
    if (b == BiomeType::Mediterranean || (w.coastClassMap.size()==w.heightmap.size() && w.coastClassMap[(size_t)i] > 0)) bias |= 1;
    if (!w.riverMap.empty() && w.riverMap[(size_t)i]) bias |= 2;
    if (b == BiomeType::Forest || b == BiomeType::TemperateGrassland) bias |= 4;
    if (b == BiomeType::Desert || b == BiomeType::Steppe) bias |= 8;
    if (score > 1.0f) w.startCandidates.push_back({i, score, bias});
  }
  std::sort(w.startCandidates.begin(), w.startCandidates.end(), [](const StartCandidate& a, const StartCandidate& b){
    if (a.score != b.score) return a.score > b.score;
    return a.cell < b.cell;
  });
  if (w.startCandidates.size() > 32) w.startCandidates.resize(32);
  w.startCandidateCount = static_cast<uint32_t>(w.startCandidates.size());
}

void build_mythic_candidates(World& w) {
  w.mythicCandidates.clear();
  for (int i = 0; i < w.width * w.height; ++i) {
    BiomeType b = biome_at(w, i);
    if (b == BiomeType::SnowMountain) w.mythicCandidates.push_back({GuardianSiteType::YetiLair, i, 1.0f + w.heightmap[(size_t)i]});
    if (b == BiomeType::Desert) w.mythicCandidates.push_back({GuardianSiteType::DuneNest, i, 0.8f + (1.0f - w.moistureMap[(size_t)i])});
    if (b == BiomeType::Forest || b == BiomeType::Jungle) w.mythicCandidates.push_back({GuardianSiteType::SacredGrove, i, 0.7f + w.moistureMap[(size_t)i]});
    if (w.terrainClass[(size_t)i] == static_cast<uint8_t>(TerrainClass::DeepWater)) w.mythicCandidates.push_back({GuardianSiteType::AbyssalTrench, i, 0.6f + std::abs(std::min(0.0f, w.heightmap[(size_t)i]))});
  }
  std::sort(w.mythicCandidates.begin(), w.mythicCandidates.end(), [](const MythicCandidate& a, const MythicCandidate& b){
    if (a.siteType != b.siteType) return a.siteType < b.siteType;
    if (a.score != b.score) return a.score > b.score;
    return a.cell < b.cell;
  });
  if (w.mythicCandidates.size() > 256) w.mythicCandidates.resize(256);
  w.mythicCandidateCount = static_cast<uint32_t>(w.mythicCandidates.size());
}

void spawn_biome_resources(World& w) {
  w.resourceNodes.clear();
  uint32_t nextId = 1;
  for (int y = 3; y < w.height - 3; y += 8) for (int x = 3; x < w.width - 3; x += 8) {
    int i = y * w.width + x;
    BiomeType b = biome_at(w, i);
    if (b == BiomeType::Coast) { w.resourceNodes.push_back({nextId++, ResourceNodeType::Ruins, {(float)x + 0.5f, (float)y + 0.5f}, 1200.0f, UINT16_MAX}); continue; }
    if (b == BiomeType::Forest || b == BiomeType::Jungle || b == BiomeType::Wetlands) w.resourceNodes.push_back({nextId++, ResourceNodeType::Forest, {(float)x + 0.5f, (float)y + 0.5f}, 1500.0f, UINT16_MAX});
    else if (b == BiomeType::Mountain || b == BiomeType::SnowMountain || b == BiomeType::Steppe || b == BiomeType::Desert) {
      float amount = (b == BiomeType::Mountain || b == BiomeType::SnowMountain) ? 2400.0f : 1300.0f;
      w.resourceNodes.push_back({nextId++, ResourceNodeType::Ore, {(float)x + 0.5f, (float)y + 0.5f}, amount, UINT16_MAX});
    }
    else w.resourceNodes.push_back({nextId++, ResourceNodeType::Farmable, {(float)x + 0.5f, (float)y + 0.5f}, 1400.0f, UINT16_MAX});
  }
  rebuild_mountain_regions(w);
  rebuild_mountain_deposits(w);
}

void rebuild_terrain_classes(World& w) {
  w.terrainClass.resize((size_t)w.width * (size_t)w.height);
  for (int i = 0; i < w.width * w.height; ++i) {
    float h = w.heightmap[i];
    if (h <= kWaterLevel - kShallowBand) w.terrainClass[i] = (uint8_t)TerrainClass::DeepWater;
    else if (h <= kWaterLevel + kShallowBand) w.terrainClass[i] = (uint8_t)TerrainClass::ShallowWater;
    else w.terrainClass[i] = (uint8_t)TerrainClass::Land;
  }
}

int mountain_region_id_at(const World& w, int cell) {
  if (cell < 0 || cell >= w.width * w.height || w.mountainRegionByCell.empty()) return 0;
  return w.mountainRegionByCell[static_cast<size_t>(cell)];
}

bool region_has_active_tunnel(const World& w, uint32_t regionId, uint16_t team) {
  for (const auto& e : w.undergroundEdges) {
    if (e.regionId == regionId && e.active && (e.owner == UINT16_MAX || e.owner == team)) return true;
  }
  return false;
}

void update_underground_economy(World& w, float dt) {
  w.undergroundYield = 0.0f;
  w.activeMineShafts = 0;
  w.activeTunnels = 0;
  w.undergroundDepots = 0;
  for (const auto& n : w.undergroundNodes) if (n.type == UndergroundNodeType::Depot && n.active) ++w.undergroundDepots;
  for (const auto& e : w.undergroundEdges) if (e.active) ++w.activeTunnels;

  for (const auto& b : w.buildings) {
    if (b.type != BuildingType::Mine || b.underConstruction || b.hp <= 0.0f) continue;
    const int cell = std::clamp((int)b.pos.y,0,w.height-1) * w.width + std::clamp((int)b.pos.x,0,w.width-1);
    const int rid = mountain_region_id_at(w, cell);
    if (rid <= 0) continue;
    ++w.activeMineShafts;
    const bool connected = region_has_active_tunnel(w, static_cast<uint32_t>(rid), b.team);
    const float rate = connected ? 1.35f : 0.45f;
    for (auto& d : w.deepDeposits) {
      if (d.regionId != static_cast<uint32_t>(rid) || d.remaining <= 0.0f || !d.active) continue;
      if (d.owner == UINT16_MAX) d.owner = b.team;
      if (d.owner != b.team) continue;
      float mined = std::min(d.remaining, rate * dt * 20.0f * d.richness);
      d.remaining -= mined;
      w.players[b.team].resources[ridx(Resource::Metal)] += mined * 0.18f;
      w.undergroundYield += mined;
      break;
    }
  }

  w.mountainRegionCount = static_cast<uint32_t>(w.mountainRegions.size());
  w.mountainChainCount = w.mountainRegionCount;
  w.surfaceDepositCount = static_cast<uint32_t>(w.surfaceDeposits.size());
  w.deepDepositCount = static_cast<uint32_t>(w.deepDeposits.size());
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

int spatial_cols(const World& w) { return std::max(1, (int)std::ceil((float)w.width / gSpatial.cellSize)); }
int spatial_rows(const World& w) { return std::max(1, (int)std::ceil((float)w.height / gSpatial.cellSize)); }
int spatial_index(const World& w, glm::vec2 p) {
  int cx = std::clamp((int)std::floor(p.x / gSpatial.cellSize), 0, spatial_cols(w) - 1);
  int cy = std::clamp((int)std::floor(p.y / gSpatial.cellSize), 0, spatial_rows(w) - 1);
  return cy * spatial_cols(w) + cx;
}
void spatial_prepare(const World& w) {
  int count = spatial_cols(w) * spatial_rows(w);
  if ((int)gSpatial.cells.size() != count) gSpatial.cells.assign(count, {});
  for (auto& c : gSpatial.cells) { c.unitIds.clear(); c.buildingIds.clear(); }
  gSpatial.unitIndexById.clear();
  gSpatial.buildingIndexById.clear();
  for (size_t i = 0; i < w.units.size(); ++i) { const auto& u = w.units[i]; if (u.hp <= 0) continue; gSpatial.unitIndexById[u.id] = i; gSpatial.cells[spatial_index(w, u.pos)].unitIds.push_back(u.id); }
  for (size_t i = 0; i < w.buildings.size(); ++i) { const auto& b = w.buildings[i]; if (b.hp <= 0.0f || b.underConstruction) continue; gSpatial.buildingIndexById[b.id] = i; gSpatial.cells[spatial_index(w, b.pos)].buildingIds.push_back(b.id); }
  for (auto& c : gSpatial.cells) { std::sort(c.unitIds.begin(), c.unitIds.end()); std::sort(c.buildingIds.begin(), c.buildingIds.end()); }
}
void spatial_collect_cells(const World& w, const glm::vec2& mn, const glm::vec2& mx) {
  int cols = spatial_cols(w), rows = spatial_rows(w);
  int minX = std::clamp((int)std::floor(mn.x / gSpatial.cellSize), 0, cols - 1);
  int maxX = std::clamp((int)std::floor(mx.x / gSpatial.cellSize), 0, cols - 1);
  int minY = std::clamp((int)std::floor(mn.y / gSpatial.cellSize), 0, rows - 1);
  int maxY = std::clamp((int)std::floor(mx.y / gSpatial.cellSize), 0, rows - 1);
  gSpatial.queryCells.clear();
  for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) gSpatial.queryCells.push_back(y * cols + x);
}
void spatial_query_radius(const World& w, glm::vec2 center, float radius, bool includeBuildings) {
  spatial_collect_cells(w, center - glm::vec2{radius, radius}, center + glm::vec2{radius, radius});
  gSpatial.queryUnits.clear(); gSpatial.queryBuildings.clear();
  for (int idx : gSpatial.queryCells) {
    const auto& c = gSpatial.cells[idx];
    gSpatial.queryUnits.insert(gSpatial.queryUnits.end(), c.unitIds.begin(), c.unitIds.end());
    if (includeBuildings) gSpatial.queryBuildings.insert(gSpatial.queryBuildings.end(), c.buildingIds.begin(), c.buildingIds.end());
  }
}
void spatial_query_aabb(const World& w, const glm::vec2& mn, const glm::vec2& mx) {
  spatial_collect_cells(w, mn, mx);
  gSpatial.queryUnits.clear();
  for (int idx : gSpatial.queryCells) {
    const auto& c = gSpatial.cells[idx];
    gSpatial.queryUnits.insert(gSpatial.queryUnits.end(), c.unitIds.begin(), c.unitIds.end());
  }
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
  TerrainClass tf = terrain_class_at(w, fromCell);
  TerrainClass tt = terrain_class_at(w, toCell);
  if (is_water_class(tf) != is_water_class(tt)) return kInfCost;
  return 10 + slope + (tt == TerrainClass::ShallowWater ? 2 : 0);
}

FlowField* get_flow_field(World& w, int targetCell) {
  for (auto& f : gNav.cache) {
    if (f.targetCell == targetCell && f.navVersion == w.navVersion && f.width == w.width && f.height == w.height) {
      ++w.flowFieldCacheHitCount;
      return &f;
    }
  }
  queue_flow_field_request(w, targetCell);
  return nullptr;
}

void recompute_population(World& w) {
  for (auto& p : w.players) { p.popUsed = 0; p.popCap = 0; }
  for (const auto& b : w.buildings) {
    if (b.underConstruction) continue;
    w.players[b.team].popCap += gBuildDefs[bidx(b.type)].popCapBonus;
  }
  for (const auto& u : w.units) if (u.hp > 0) w.players[u.team].popUsed += gUnitDefs[uidx(u.type)].popCost;
}


bool valid_mine_shaft_placement_impl(const World& w, glm::ivec2 tile) {
  if (tile.x < 0 || tile.y < 0 || tile.x >= w.width || tile.y >= w.height) return false;
  const int cell = tile.y * w.width + tile.x;
  if (w.mountainRegionByCell.empty()) return false;
  if (w.mountainRegionByCell[static_cast<size_t>(cell)] <= 0) return false;
  return biome_at(w, cell) == BiomeType::Mountain || biome_at(w, cell) == BiomeType::SnowMountain;
}

bool deep_deposit_available_impl(const World& w, uint32_t depositId, uint16_t team) {
  for (const auto& d : w.deepDeposits) {
    if (d.id != depositId) continue;
    if (!d.active || d.remaining <= 0.0f) return false;
    return d.owner == UINT16_MAX || d.owner == team;
  }
  return false;
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
  if (type == BuildingType::Port) {
    if (terrain_class_at(w, ty * w.width + tx) != TerrainClass::Land || !has_adjacent_coast(w, pos)) return false;
  } else if (type == BuildingType::Mine) {
    if (!valid_mine_shaft_placement_impl(w, {tx, ty})) return false;
  } else if (is_water_class(terrain_class_at(w, ty * w.width + tx))) return false;
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
  ensure_chunk_layout(w);
  std::vector<std::vector<std::pair<int, uint16_t>>> chunkWrites(gChunks.chunks.size());
  TaskGraph graph;
  for (size_t chunkIdx = 0; chunkIdx < gChunks.chunks.size(); ++chunkIdx) {
    graph.jobs.push_back({[&w, &chunkWrites, chunkIdx]() {
      const int cx = static_cast<int>(chunkIdx) % gChunks.cols;
      const int cy = static_cast<int>(chunkIdx) / gChunks.cols;
      const int minX = cx * gChunks.chunkWidth;
      const int minY = cy * gChunks.chunkHeight;
      const int maxX = std::min(w.width, minX + gChunks.chunkWidth);
      const int maxY = std::min(w.height, minY + gChunks.chunkHeight);
      auto& out = chunkWrites[chunkIdx];
      out.reserve(static_cast<size_t>((maxX - minX) * (maxY - minY)));
      for (int y = minY; y < maxY; ++y) {
        for (int x = minX; x < maxX; ++x) {
          float best = 1e9f;
          uint16_t owner = 0;
          glm::vec2 p{x + 0.5f, y + 0.5f};
          for (const auto& c : w.cities) {
            float d = dist(c.pos, p) / (c.capital ? 1.4f : 1.0f);
            if (d < best && d < 22.0f + c.level * 2.0f) { best = d; owner = c.team; }
          }
          out.push_back({y * w.width + x, owner});
        }
      }
    }});
  }
  gLastStats.territoryTasks += static_cast<uint32_t>(graph.jobs.size());
  gLastStats.jobCount += static_cast<uint32_t>(graph.jobs.size());
  run_task_graph(graph);
  std::fill(w.territoryOwner.begin(), w.territoryOwner.end(), 0);
  for (size_t i = 0; i < chunkWrites.size(); ++i) {
    for (const auto& entry : chunkWrites[i]) w.territoryOwner[entry.first] = entry.second;
  }
  ++w.territoryRecomputeCount;
  w.territoryDirty = true;
}

float unit_vision_radius(const Unit& u) {
  float radius = 7.0f;
  if (u.type == UnitType::Cavalry) radius = 8.5f;
  else if (u.type == UnitType::Worker) radius = 6.2f;
  else if (u.type == UnitType::TransportShip) radius = 9.5f;
  else if (u.type == UnitType::LightWarship) radius = 10.0f;
  else if (u.type == UnitType::HeavyWarship) radius = 8.0f;
  else if (u.type == UnitType::BombardShip) radius = 8.5f;
  if (unit_is_recon(u)) radius += 4.0f;
  return radius;
}

float building_vision_radius(BuildingType type) {
  if (type == BuildingType::CityCenter) return 11.0f;
  if (type == BuildingType::Port) return 12.0f;
  if (type == BuildingType::Wonder) return 10.0f;
  if (type == BuildingType::Library) return 12.5f;
  return 7.0f;
}

bool unit_is_recon(const Unit& u) {
  return u.type == UnitType::Cavalry || u.type == UnitType::TransportShip || u.type == UnitType::LightWarship || (u.type == UnitType::Worker && u.team != 0);
}

bool unit_has_stealth(const Unit& u) {
  return u.type == UnitType::TransportShip || u.type == UnitType::BombardShip || (u.type == UnitType::Worker && u.team != 0);
}

float unit_detection_radius(const Unit& u) {
  if (u.type == UnitType::LightWarship) return 10.5f;
  if (u.type == UnitType::Worker) return 9.0f;
  if (u.type == UnitType::Cavalry) return 7.5f;
  return 0.0f;
}


bool unit_is_air(UnitType t) {
  return t == UnitType::Fighter || t == UnitType::Interceptor || t == UnitType::Bomber || t == UnitType::StrategicBomber || t == UnitType::ReconDrone || t == UnitType::StrikeDrone || t == UnitType::TacticalMissile || t == UnitType::StrategicMissile;
}

AirUnitClass air_class_for(UnitType t) {
  if (t == UnitType::Interceptor) return AirUnitClass::Interceptor;
  if (t == UnitType::Bomber) return AirUnitClass::Bomber;
  if (t == UnitType::StrategicBomber) return AirUnitClass::StrategicBomber;
  if (t == UnitType::ReconDrone) return AirUnitClass::ReconDrone;
  if (t == UnitType::StrikeDrone) return AirUnitClass::StrikeDrone;
  return AirUnitClass::Fighter;
}

float detector_radius(DetectorType t) {
  if (t == DetectorType::AirbaseRadar) return 18.0f;
  if (t == DetectorType::SatelliteUplink) return 30.0f;
  if (t == DetectorType::NavalSensor) return 12.0f;
  if (t == DetectorType::AntiMissileDefense) return 10.0f;
  if (t == DetectorType::AABattery) return 9.0f;
  return 14.0f;
}

void rebuild_detector_sites(World& w) {
  w.detectors.clear();
  uint32_t nextId = 1;
  for (const auto& b : w.buildings) {
    if (b.hp <= 0.0f || b.underConstruction) continue;
    DetectorType dt{}; bool ok = true; bool contact = false;
    if (b.type == BuildingType::RadarTower) dt = DetectorType::RadarTower;
    else if (b.type == BuildingType::MobileRadar) { dt = DetectorType::MobileRadar; contact = true; }
    else if (b.type == BuildingType::Airbase) dt = DetectorType::AirbaseRadar;
    else if (b.type == BuildingType::AABattery) dt = DetectorType::AABattery;
    else if (b.type == BuildingType::AntiMissileDefense) dt = DetectorType::AntiMissileDefense;
    else ok = false;
    if (ok) w.detectors.push_back({nextId++, b.team, dt, b.pos, detector_radius(dt), contact, true});
  }
  for (const auto& u : w.units) {
    if (u.hp <= 0 || u.embarked) continue;
    if (u.type == UnitType::LightWarship || u.type == UnitType::HeavyWarship) w.detectors.push_back({nextId++, u.team, DetectorType::NavalSensor, u.pos, detector_radius(DetectorType::NavalSensor), true, true});
    if (u.type == UnitType::ReconDrone) w.detectors.push_back({nextId++, u.team, DetectorType::ReconDrone, u.pos, 11.0f, false, true});
  }
}

void update_air_and_strategic_warfare(World& w, float dt) {
  for (auto& z : w.denialZones) if (z.ticksRemaining > 0) --z.ticksRemaining;
  w.denialZones.erase(std::remove_if(w.denialZones.begin(), w.denialZones.end(), [](const DenialZone& z){ return z.ticksRemaining == 0; }), w.denialZones.end());

  for (auto& a : w.airUnits) {
    if (a.cooldownTicks > 0) --a.cooldownTicks;
    glm::vec2 target = a.state == AirMissionState::Returning ? glm::vec2{20.0f + a.team * 75.0f, 20.0f + a.team * 75.0f} : a.missionTarget;
    glm::vec2 d = target - a.pos;
    float len = glm::length(d);
    if (len > 0.1f) a.pos += (d / len) * a.speed * dt;
    a.pos.x = std::clamp(a.pos.x, 0.0f, (float)w.width - 0.01f);
    a.pos.y = std::clamp(a.pos.y, 0.0f, (float)w.height - 0.01f);
    if (len < 1.5f && a.state == AirMissionState::Airborne) a.state = AirMissionState::Attacking;
    else if (len < 1.5f && a.state == AirMissionState::Attacking) { a.state = AirMissionState::Returning; a.missionPerformed = true; ++w.airMissionEvents; }
    else if (len < 2.0f && a.state == AirMissionState::Returning) { a.state = AirMissionState::Airborne; }
  }

  for (auto& s : w.strategicStrikes) {
    if (s.resolved) continue;
    if (s.prepTicksRemaining > 0) { --s.prepTicksRemaining; continue; }
    if (!s.launched) { s.launched = true; ++w.strategicStrikeEvents; continue; }
    if (s.travelTicksRemaining > 0) { --s.travelTicksRemaining; continue; }
    int defensive = 0;
    for (const auto& d : w.detectors) {
      if ((d.type == DetectorType::AntiMissileDefense || d.type == DetectorType::AABattery) && d.team != s.team && glm::length(d.pos - s.target) <= d.radius) ++defensive;
    }
    const int roll = (int)((s.id * 1103515245u + w.tick * 97u + s.team * 13u) % 100);
    if (defensive == 0) s.interceptionState = 0;
    else if (roll < 25) s.interceptionState = 1;
    else if (roll < 65) s.interceptionState = 2;
    else s.interceptionState = 3;
    if (s.interceptionState > 0) ++w.interceptionEvents;
    if (s.interceptionState <= 1) {
      for (auto& u : w.units) if (u.hp > 0 && glm::length(u.pos - s.target) < 5.0f) u.hp -= s.type == StrikeType::StrategicMissile ? 160.0f : 90.0f;
      for (auto& b : w.buildings) if (b.hp > 0 && glm::length(b.pos - s.target) < 6.0f) b.hp -= s.type == StrikeType::StrategicMissile ? 480.0f : 250.0f;
      w.denialZones.push_back({(uint32_t)(w.denialZones.size()+1), s.team, s.target, s.type == StrikeType::StrategicMissile ? 9.0f : 6.0f, s.type == StrikeType::StrategicMissile ? 220u : 120u});
      w.worldTension = std::min(100.0f, w.worldTension + (s.type == StrikeType::StrategicMissile ? 7.0f : 3.0f));
    }
    s.resolved = true;
  }
}

void recompute_fog(World& w) {
  const int cells = w.width * w.height;
  const size_t playerCount = w.players.size();
  w.fogVisibilityByPlayer.assign(playerCount * static_cast<size_t>(cells), 0);
  if (w.fogExploredByPlayer.size() != playerCount * static_cast<size_t>(cells)) {
    w.fogExploredByPlayer.assign(playerCount * static_cast<size_t>(cells), 0);
  }
  w.fogMaskByPlayer.assign(playerCount * static_cast<size_t>(cells), 0);
  if (w.fog.size() != static_cast<size_t>(cells)) w.fog.assign(cells, 0);

  if (w.godMode) {
    std::fill(w.fog.begin(), w.fog.end(), 0);
    std::fill(w.fogVisibilityByPlayer.begin(), w.fogVisibilityByPlayer.end(), 255);
    std::fill(w.fogExploredByPlayer.begin(), w.fogExploredByPlayer.end(), 255);
    std::fill(w.fogMaskByPlayer.begin(), w.fogMaskByPlayer.end(), 0);
    w.fogDirty = true;
    return;
  }

  auto reveal_disc = [&](uint16_t player, glm::vec2 center, float radius) {
    if (player >= playerCount) return;
    const int minX = std::max(0, static_cast<int>(std::floor(center.x - radius)));
    const int maxX = std::min(w.width - 1, static_cast<int>(std::ceil(center.x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(center.y - radius)));
    const int maxY = std::min(w.height - 1, static_cast<int>(std::ceil(center.y + radius)));
    const float r2 = radius * radius;
    const size_t base = static_cast<size_t>(player) * static_cast<size_t>(cells);
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        const float dx = (x + 0.5f) - center.x;
        const float dy = (y + 0.5f) - center.y;
        if (dx * dx + dy * dy > r2) continue;
        const size_t idx = base + static_cast<size_t>(y * w.width + x);
        w.fogVisibilityByPlayer[idx] = 255;
      }
    }
  };

  for (const auto& b : w.buildings) {
    if (b.hp <= 0.0f || b.underConstruction) continue;
    reveal_disc(b.team, b.pos, building_vision_radius(b.type));
  }
  for (const auto& u : w.units) {
    if (u.hp <= 0.0f || u.embarked) continue;
    reveal_disc(u.team, u.pos, unit_vision_radius(u));
  }
  for (const auto& a : w.airUnits) reveal_disc(a.team, a.pos, a.cls == AirUnitClass::ReconDrone ? 12.0f : 7.0f);

  w.radarContactByPlayer.assign(playerCount * static_cast<size_t>(cells), 0);
  for (const auto& d : w.detectors) {
    if (!d.active || d.team >= playerCount) continue;
    const size_t base = static_cast<size_t>(d.team) * static_cast<size_t>(cells);
    for (const auto& air : w.airUnits) {
      if (air.team == d.team || players_allied(w, air.team, d.team)) continue;
      if (glm::length(air.pos - d.pos) > d.radius) continue;
      int tile = cell_of(w, air.pos);
      w.radarContactByPlayer[base + static_cast<size_t>(tile)] = 255;
      ++w.radarRevealEvents;
      if (!d.revealContactOnly) w.fogVisibilityByPlayer[base + static_cast<size_t>(tile)] = 255;
    }
  }

  for (size_t p = 0; p < playerCount; ++p) {
    const size_t base = p * static_cast<size_t>(cells);
    for (int i = 0; i < cells; ++i) {
      if (w.fogVisibilityByPlayer[base + static_cast<size_t>(i)] > 0) w.fogExploredByPlayer[base + static_cast<size_t>(i)] = 255;
      const bool vis = w.fogVisibilityByPlayer[base + static_cast<size_t>(i)] > 0;
      const bool exp = w.fogExploredByPlayer[base + static_cast<size_t>(i)] > 0;
      w.fogMaskByPlayer[base + static_cast<size_t>(i)] = vis ? 0 : (exp ? 128 : 255);
    }
  }

  const size_t localBase = playerCount > 0 ? 0 : 0;
  for (int i = 0; i < cells; ++i) w.fog[i] = w.fogMaskByPlayer[localBase + static_cast<size_t>(i)];
  w.fogDirty = true;
}

void ensure_chunk_layout(const World& w) {
  gChunks.cols = std::max(1, (w.width + gChunks.chunkWidth - 1) / gChunks.chunkWidth);
  gChunks.rows = std::max(1, (w.height + gChunks.chunkHeight - 1) / gChunks.chunkHeight);
  const int count = gChunks.cols * gChunks.rows;
  if ((int)gChunks.chunks.size() != count) gChunks.chunks.assign(count, {});
}

int chunk_index_of_tile(const World& w, int tx, int ty) {
  ensure_chunk_layout(w);
  const int cx = std::clamp(tx / gChunks.chunkWidth, 0, gChunks.cols - 1);
  const int cy = std::clamp(ty / gChunks.chunkHeight, 0, gChunks.rows - 1);
  return cy * gChunks.cols + cx;
}

int chunk_index_of_pos(const World& w, glm::vec2 p) {
  const int tx = std::clamp((int)p.x, 0, w.width - 1);
  const int ty = std::clamp((int)p.y, 0, w.height - 1);
  return chunk_index_of_tile(w, tx, ty);
}

void rebuild_chunk_membership_impl(const World& w) {
  ensure_chunk_layout(w);
  for (auto& c : gChunks.chunks) {
    c.unitIds.clear();
    c.buildingIds.clear();
    c.resourceIds.clear();
    c.fogTiles.clear();
    c.territoryTiles.clear();
  }
  for (const auto& u : w.units) {
    if (u.hp <= 0 || u.embarked) continue;
    gChunks.chunks[chunk_index_of_pos(w, u.pos)].unitIds.push_back(u.id);
  }
  for (const auto& b : w.buildings) {
    if (b.hp <= 0.0f) continue;
    gChunks.chunks[chunk_index_of_pos(w, b.pos)].buildingIds.push_back(b.id);
  }
  for (const auto& r : w.resourceNodes) {
    if (r.amount <= 0.0f) continue;
    gChunks.chunks[chunk_index_of_pos(w, r.pos)].resourceIds.push_back(r.id);
  }
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      const int tile = y * w.width + x;
      auto& c = gChunks.chunks[chunk_index_of_tile(w, x, y)];
      c.fogTiles.push_back(tile);
      c.territoryTiles.push_back(tile);
    }
  }
  for (auto& c : gChunks.chunks) {
    std::sort(c.unitIds.begin(), c.unitIds.end());
    std::sort(c.buildingIds.begin(), c.buildingIds.end());
    std::sort(c.resourceIds.begin(), c.resourceIds.end());
    std::sort(c.fogTiles.begin(), c.fogTiles.end());
    std::sort(c.territoryTiles.begin(), c.territoryTiles.end());
  }
}

struct MovementResult {
  bool valid{false};
  uint32_t id{0};
  glm::vec2 pos{};
  glm::vec2 moveDir{};
  uint16_t stuckTicks{0};
  bool reachedSlot{false};
};

void enqueue_nav_request(World& w, int targetCell) {
  NavRequest req{};
  req.requestId = gNextNavRequestId++;
  req.tickIssued = w.tick;
  req.navVersion = w.navVersion;
  req.targetCell = targetCell;
  gPendingNavRequests.push_back(req);
}

void queue_flow_field_request(World& w, int targetCell) {
  for (const auto& f : gNav.cache) {
    if (f.targetCell == targetCell && f.navVersion == w.navVersion && f.width == w.width && f.height == w.height) return;
  }
  for (const auto& p : gPendingNavRequests) {
    if (p.targetCell == targetCell && p.navVersion == w.navVersion) return;
  }
  enqueue_nav_request(w, targetCell);
}

void process_nav_requests(World& w) {
  if (gPendingNavRequests.empty()) return;
  std::vector<NavRequest> requests = std::move(gPendingNavRequests);
  std::sort(requests.begin(), requests.end(), [](const NavRequest& a, const NavRequest& b) { return a.requestId < b.requestId; });

  TaskGraph navGraph;
  std::mutex doneMutex;
  for (const auto& req : requests) {
    navGraph.jobs.push_back({[&w, &doneMutex, req]() {
      if (req.navVersion != w.navVersion) return;
      FlowField f{};
      f.targetCell = req.targetCell;
      f.navVersion = req.navVersion;
      f.width = w.width;
      f.height = w.height;
      const int cells = w.width * w.height;
      f.integration.assign(cells, kInfCost);
      f.dirX.assign(cells, 0);
      f.dirY.assign(cells, 0);
      std::vector<uint8_t> blocked(cells, 0);
      for (const auto& b : w.buildings) {
        int minX = std::max(0, (int)std::floor(b.pos.x - b.size.x * 0.5f));
        int maxX = std::min(w.width - 1, (int)std::ceil(b.pos.x + b.size.x * 0.5f));
        int minY = std::max(0, (int)std::floor(b.pos.y - b.size.y * 0.5f));
        int maxY = std::min(w.height - 1, (int)std::ceil(b.pos.y + b.size.y * 0.5f));
        for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) blocked[y * w.width + x] = 1;
      }
      std::queue<int> q;
      f.integration[req.targetCell] = 0;
      q.push(req.targetCell);
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
          if (blocked[ni]) continue;
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
      std::lock_guard<std::mutex> lock(doneMutex);
      gCompletedNavResults.push_back({req.requestId, req.navVersion, std::move(f)});
    }});
  }
  gLastStats.navRequests += static_cast<uint32_t>(requests.size());
  gLastStats.jobCount += static_cast<uint32_t>(navGraph.jobs.size());
  run_task_graph(navGraph);
}

void apply_nav_results(World& w) {
  if (gCompletedNavResults.empty()) return;
  std::sort(gCompletedNavResults.begin(), gCompletedNavResults.end(), [](const NavCompletion& a, const NavCompletion& b) {
    return a.requestId < b.requestId;
  });
  for (auto& done : gCompletedNavResults) {
    if (done.navVersion != w.navVersion) {
      ++gLastStats.navStaleDrops;
      continue;
    }
    bool replaced = false;
    for (auto& cached : gNav.cache) {
      if (cached.targetCell == done.field.targetCell && cached.navVersion == done.field.navVersion && cached.width == done.field.width && cached.height == done.field.height) {
        cached = std::move(done.field);
        replaced = true;
        break;
      }
    }
    if (!replaced) gNav.cache.push_back(std::move(done.field));
    ++w.flowFieldGeneratedCount;
    ++gLastStats.navCompletions;
  }
  if (gNav.cache.size() > 32) gNav.cache.erase(gNav.cache.begin(), gNav.cache.begin() + (gNav.cache.size() - 32));
  gCompletedNavResults.clear();
}



uint32_t find_enemy_near(const World& w, const Unit& u, float radius) {
  spatial_query_radius(w, u.pos, radius, false);
  uint32_t bestId = 0;
  int bestScore = -1;
  for (uint32_t id : gSpatial.queryUnits) {
    const Unit* e = nullptr;
    auto it = gSpatial.unitIndexById.find(id); if (it != gSpatial.unitIndexById.end()) e = &w.units[it->second];
    if (!e || players_allied(w, e->team, u.team) || e->hp <= 0) continue;
    if (!is_unit_visible_to_player(w, *e, u.team)) continue;
    float d = dist(u.pos, e->pos);
    if (d > radius) continue;
    int score = 5000 - static_cast<int>(d * 800.0f);
    if (e->hp < 40.0f) score += 250;
    if (e->role == u.preferredTargetRole) score += 400;
    score += static_cast<int>(u.vsRoleMultiplierPermille[role_idx(e->role)]) - 1000;
    if (u.role == UnitRole::Siege && e->role == UnitRole::Building) score += 600;
    if (score > bestScore || (score == bestScore && e->id < bestId)) { bestScore = score; bestId = e->id; }
  }
  return bestId;
}

int find_building_target(const World& w, const Unit& u, float radius) {
  spatial_query_radius(w, u.pos, radius, true);
  int best = -1;
  int bestScore = -1;
  for (uint32_t id : gSpatial.queryBuildings) {
    auto it = gSpatial.buildingIndexById.find(id);
    if (it == gSpatial.buildingIndexById.end()) continue;
    int i = static_cast<int>(it->second);
    const auto& b = w.buildings[i];
    if (players_allied(w, b.team, u.team) || b.underConstruction || b.hp <= 0.0f) continue;
    float d = dist(u.pos, b.pos);
    if (d > radius) continue;
    int score = 4800 - static_cast<int>(d * 800.0f);
    if (u.role == UnitRole::Siege) score += 900;
    if (score > bestScore || (score == bestScore && (best < 0 || b.id < w.buildings[best].id))) { bestScore = score; best = i; }
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

std::string resolved_unit_definition_id(const World& w, uint16_t team, UnitType type) {
  if (team >= w.players.size()) return unit_name(type);
  const auto& v = w.players[team].civilization.uniqueUnitDefs[static_cast<size_t>(type)];
  return v.empty() ? std::string(unit_name(type)) : v;
}

std::string resolved_building_definition_id(const World& w, uint16_t team, BuildingType type) {
  if (team >= w.players.size()) return building_name(type);
  const auto& v = w.players[team].civilization.uniqueBuildingDefs[static_cast<size_t>(type)];
  return v.empty() ? std::string(building_name(type)) : v;
}

uint32_t spawn_unit(World& w, uint16_t team, UnitType type, glm::vec2 p) {
  uint32_t id = 1;
  for (const auto& u : w.units) id = std::max(id, u.id + 1);
  Unit nu{}; nu.id = id; nu.team = team; nu.type = type; nu.pos = nu.renderPos = nu.target = nu.slotTarget = p;
  const UnitDef& ud = gUnitDefs[uidx(type)];
  const auto& civ = team < w.players.size() ? w.players[team].civilization : CivilizationRuntime{};
  nu.role = ud.role;
  nu.attackType = ud.attackType;
  nu.preferredTargetRole = ud.preferredTargetRole;
  nu.vsRoleMultiplierPermille = ud.vsRoleMultiplierPermille;
  nu.attackCooldownTicks = 0;
  if (type == UnitType::Worker) { nu.hp = 70; nu.attack = 3.0f; nu.range = 1.5f; nu.speed = 4.3f; }
  else if (type == UnitType::Infantry) { nu.hp = 105; nu.attack = 8.5f; nu.range = 2.0f; nu.speed = 4.8f; }
  else if (type == UnitType::Archer) { nu.hp = 80; nu.attack = 7.0f; nu.range = 5.4f; nu.speed = 4.4f; }
  else if (type == UnitType::Cavalry) { nu.hp = 118; nu.attack = 6.8f; nu.range = 1.8f; nu.speed = 6.2f; }
  else if (type == UnitType::Siege) { nu.hp = 110; nu.attack = 13.0f; nu.range = 6.2f; nu.speed = 3.2f; }
  else if (type == UnitType::TransportShip) { nu.hp = 220; nu.attack = 1.5f; nu.range = 2.2f; nu.speed = 4.8f; nu.role = UnitRole::Transport; }
  else if (type == UnitType::LightWarship) { nu.hp = 190; nu.attack = 8.0f; nu.range = 4.8f; nu.speed = 5.4f; nu.role = UnitRole::Naval; nu.preferredTargetRole = UnitRole::Transport; nu.vsRoleMultiplierPermille = {1000,1000,1000,1000,1000,1000,1100,1500}; }
  else if (type == UnitType::HeavyWarship) { nu.hp = 280; nu.attack = 16.0f; nu.range = 4.6f; nu.speed = 4.0f; nu.role = UnitRole::Naval; nu.preferredTargetRole = UnitRole::Naval; nu.vsRoleMultiplierPermille = {1000,1000,1000,1000,1000,1000,1450,1000}; }
  else if (type == UnitType::BombardShip) { nu.hp = 240; nu.attack = 18.0f; nu.range = 7.2f; nu.speed = 3.6f; nu.role = UnitRole::Naval; nu.preferredTargetRole = UnitRole::Building; nu.vsRoleMultiplierPermille = {900,900,900,1000,900,1700,1000,1000}; }
  else if (type == UnitType::Fighter || type == UnitType::Interceptor) { nu.hp = 90; nu.attack = 12.0f; nu.range = 4.8f; nu.speed = 8.4f; nu.role = UnitRole::Ranged; }
  else if (type == UnitType::Bomber || type == UnitType::StrategicBomber) { nu.hp = 120; nu.attack = 22.0f; nu.range = 6.0f; nu.speed = 6.3f; nu.role = UnitRole::Siege; nu.preferredTargetRole = UnitRole::Building; }
  else if (type == UnitType::ReconDrone || type == UnitType::StrikeDrone) { nu.hp = 60; nu.attack = type == UnitType::StrikeDrone ? 9.0f : 3.0f; nu.range = 5.4f; nu.speed = 8.8f; nu.role = UnitRole::Ranged; }
  else if (type == UnitType::TacticalMissile || type == UnitType::StrategicMissile) { nu.hp = 30; nu.attack = 40.0f; nu.range = 2.0f; nu.speed = 10.0f; nu.role = UnitRole::Siege; }
  nu.attack *= civ.unitAttackMult[static_cast<size_t>(type)];
  nu.hp *= civ.unitHpMult[static_cast<size_t>(type)];
  nu.definitionId = resolved_unit_definition_id(w, team, type);
  if (!unit_cell_valid(w, nu, cell_of(w, nu.pos))) {
    for (int y = 0; y < w.height; ++y) for (int x = 0; x < w.width; ++x) {
      int c = y * w.width + x;
      if (unit_cell_valid(w, nu, c)) { nu.pos = nu.renderPos = nu.target = nu.slotTarget = {x + 0.5f, y + 0.5f}; y = w.height; break; }
    }
  }
  w.units.push_back(nu);
  return id;
}

uint16_t guardian_default_owner(const World& w, const GuardianDefinition& def) {
  if (w.players.empty()) return 0;
  if (def.joinMode == GuardianJoinMode::NeverJoin || def.behaviorMode == GuardianBehaviorMode::HostileEncounter) {
    return static_cast<uint16_t>(w.players.size() > 1 ? 1 : 0);
  }
  return 0;
}

uint32_t spawn_guardian_unit(World& w, GuardianSiteInstance& site, const GuardianDefinition& def) {
  const uint16_t owner = site.owner == UINT16_MAX ? guardian_default_owner(w, def) : site.owner;
  const uint32_t uid = spawn_unit(w, owner, UnitType::Siege, site.pos + glm::vec2{0.5f, 0.0f});
  for (auto& u : w.units) {
    if (u.id != uid) continue;
    u.hp = def.unitHp;
    u.attack = def.unitAttack;
    u.range = def.unitRange;
    u.speed = def.unitSpeed;
    u.role = UnitRole::Siege;
    u.definitionId = def.associatedUnitDefinitionId.empty() ? def.guardianId : def.associatedUnitDefinitionId;
    if (def.guardianId == "kraken") {
      u.role = UnitRole::Naval;
      u.preferredTargetRole = UnitRole::Transport;
      u.vsRoleMultiplierPermille = {900,900,900,900,900,1800,1700,1300};
    } else if (def.guardianId == "sandworm") {
      u.role = UnitRole::Cavalry;
      u.preferredTargetRole = UnitRole::Worker;
      u.vsRoleMultiplierPermille = {1000,1450,1100,1200,1000,1500,1000,1000};
    } else if (def.guardianId == "forest_spirit") {
      u.role = UnitRole::Ranged;
      u.preferredTargetRole = UnitRole::Infantry;
      u.vsRoleMultiplierPermille = {1150,1100,1100,1000,900,1000,1000,1000};
    } else {
      u.preferredTargetRole = UnitRole::Building;
      u.vsRoleMultiplierPermille = {1300,1600,1200,1200,1000,1900,1000,1000};
    }
    break;
  }
  return uid;
}

bool assign_guardian_owner(World& world, uint32_t siteInstanceId, uint16_t player) {
  const int idx = find_guardian_site_index(world, siteInstanceId);
  if (idx < 0 || player >= world.players.size()) return false;
  auto& site = world.guardianSites[static_cast<size_t>(idx)];
  site.owner = player;
  for (auto& u : world.units) {
    if (u.definitionId != site.guardianId) continue;
    if (dist(u.pos, site.pos) > 6.0f) continue;
    u.team = player;
    break;
  }
  return true;
}

bool activate_guardian_site(World& world, uint32_t siteInstanceId) {
  const int idx = find_guardian_site_index(world, siteInstanceId);
  if (idx < 0) return false;
  auto& site = world.guardianSites[static_cast<size_t>(idx)];
  const GuardianDefinition* def = find_guardian_definition(world, site.guardianId);
  if (!def || site.spawned || site.oneShotUsed) return false;
  if (site.owner == UINT16_MAX) site.owner = guardian_default_owner(world, *def);
  spawn_guardian_unit(world, site, *def);
  site.spawned = true;
  site.siteActive = false;
  site.siteDepleted = true;
  ++world.guardiansSpawned;
  const bool allied = def->joinMode == GuardianJoinMode::DiscovererControl && site.owner < world.players.size();
  if (allied) {
    ++world.guardiansJoined;
    ++world.alliedGuardianEvents;
    emit_event(world, GameplayEventType::GuardianJoined, site.owner, site.owner, site.instanceId, def->displayName + " joined player");
  } else {
    ++world.hostileGuardianEvents;
    emit_event(world, GameplayEventType::GuardianDiscovered, site.owner, site.owner, site.instanceId, def->displayName + " awakened hostile");
  }
  return true;
}

bool reveal_guardian_site(World& world, uint32_t siteInstanceId, uint16_t discoverer) {
  const int idx = find_guardian_site_index(world, siteInstanceId);
  if (idx < 0) return false;
  auto& site = world.guardianSites[static_cast<size_t>(idx)];
  const GuardianDefinition* def = find_guardian_definition(world, site.guardianId);
  if (!def) return false;
  if (site.discovered) return true;
  site.discovered = true;
  if (def->joinMode == GuardianJoinMode::DiscovererControl) site.owner = discoverer;
  else if (site.owner == UINT16_MAX) site.owner = guardian_default_owner(world, *def);
  ++world.guardiansDiscovered;
  emit_event(world, GameplayEventType::GuardianDiscovered, discoverer, discoverer, site.instanceId,
    "Mythic guardian discovered: " + def->displayName);
  if (def->spawnMode == GuardianSpawnMode::OnDiscovery) {
    activate_guardian_site(world, site.instanceId);
    if (def->joinMode == GuardianJoinMode::DiscovererControl) assign_guardian_owner(world, site.instanceId, discoverer);
  }
  return true;
}

void generate_guardian_sites(World& w) {
  uint32_t nextId = 1;
  for (const auto& s : w.guardianSites) nextId = std::max(nextId, s.instanceId + 1);
  std::mt19937 rng(w.seed ^ 0x9e3779b9u);
  for (const auto& d : w.guardianDefinitions) {
    if (!d.procedural || d.scenarioOnly) continue;
    uint32_t existing = 0;
    for (const auto& s : w.guardianSites) if (s.guardianId == d.guardianId) ++existing;
    if (d.unique && existing > 0) continue;
    if (existing >= d.maxPerMap) continue;
    std::vector<int> candidates;
    for (const auto& mc : w.mythicCandidates) {
      if (static_cast<uint8_t>(biome_at(w, mc.cell)) != d.biomeRequirement) continue;
      if ((rng() % 1000) >= static_cast<uint32_t>(d.rarityPermille)) continue;
      bool spaced = true;
      glm::vec2 p{(mc.cell % w.width) + 0.5f, (mc.cell / w.width) + 0.5f};
      for (const auto& s : w.guardianSites) if (glm::length(s.pos - p) < (float)d.minSpacingCells) { spaced = false; break; }
      if (spaced) candidates.push_back(mc.cell);
    }
    std::sort(candidates.begin(), candidates.end());
    const uint32_t need = std::min<uint32_t>(d.maxPerMap - existing, d.unique ? 1u : d.maxPerMap);
    for (uint32_t k = 0; k < need && k < candidates.size(); ++k) {
      const int cell = candidates[k];
      GuardianSiteInstance s{};
      s.instanceId = nextId++;
      s.guardianId = d.guardianId;
      s.siteType = d.siteType;
      s.pos = {(cell % w.width) + 0.5f, (cell / w.width) + 0.5f};
      s.regionId = (cell >= 0 && cell < (int)w.mountainRegionByCell.size()) ? w.mountainRegionByCell[(size_t)cell] : -1;
      w.guardianSites.push_back(std::move(s));
    }
  }
  std::sort(w.guardianSites.begin(), w.guardianSites.end(), [](const GuardianSiteInstance& a, const GuardianSiteInstance& b){ return a.instanceId < b.instanceId; });
}

void update_guardian_sites(World& w) {
  for (auto& s : w.guardianSites) {
    if (!s.siteActive || s.siteDepleted || s.oneShotUsed) continue;
    const GuardianDefinition* def = find_guardian_definition(w, s.guardianId);
    if (!def) continue;
    if (s.discovered) continue;
    bool discovered = false;
    uint16_t discoverer = 0;
    for (const auto& u : w.units) {
      if (u.hp <= 0 || u.embarked) continue;
      if (glm::length(u.pos - s.pos) <= def->discoveryRadius) {
        discovered = true;
        discoverer = u.team;
        break;
      }
      const int c = cell_of(w, u.pos);
      if (def->discoveryMode == GuardianDiscoveryMode::FogReveal && c >= 0 && c < (int)w.fog.size() && w.fog[(size_t)c] == 0) {
        discovered = true;
        discoverer = u.team;
        break;
      }
      if (def->discoveryMode == GuardianDiscoveryMode::UndergroundDiscovery) {
        for (const auto& n : w.undergroundNodes) {
          if (!n.active || n.owner != u.team || (int)n.regionId != s.regionId) continue;
          discovered = true;
          discoverer = u.team;
          break;
        }
      }
      if (discovered) break;
    }
    if (discovered) reveal_guardian_site(w, s.instanceId, discoverer);
  }
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

bool road_tile_owned(const World& w, uint16_t team, int tx, int ty) {
  if (tx < 0 || ty < 0 || tx >= w.width || ty >= w.height) return false;
  const uint16_t owner = w.territoryOwner[ty * w.width + tx];
  return owner == team;
}

bool has_road_on_tile(const World& w, uint16_t team, int tx, int ty) {
  for (const auto& r : w.roads) {
    if (r.owner != team) continue;
    if ((r.a.x == tx && r.a.y == ty) || (r.b.x == tx && r.b.y == ty)) return true;
  }
  return false;
}

bool near_friendly_road(const World& w, uint16_t team, glm::vec2 p) {
  int tx = std::clamp((int)std::round(p.x), 0, w.width - 1);
  int ty = std::clamp((int)std::round(p.y), 0, w.height - 1);
  for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
    if (has_road_on_tile(w, team, tx + ox, ty + oy)) return true;
    for (uint16_t other = 0; other < w.players.size(); ++other) {
      if (other == team) continue;
      if (trade_access_allowed(w, team, other) && has_road_on_tile(w, other, tx + ox, ty + oy)) return true;
    }
  }
  return false;
}

void ensure_base_roads(World& w) {
  if (!w.roads.empty()) return;
  uint32_t nextId = 1;
  for (const auto& city : w.cities) {
    for (const auto& b : w.buildings) {
      if (b.team != city.team || b.type != BuildingType::Market) continue;
      glm::ivec2 a{(int)std::round(city.pos.x), (int)std::round(city.pos.y)};
      glm::ivec2 c{(int)std::round(b.pos.x), (int)std::round(b.pos.y)};
      int x = a.x;
      int y = a.y;
      while (x != c.x || y != c.y) {
        int nx = x + (x < c.x ? 1 : (x > c.x ? -1 : 0));
        int ny = y + (y < c.y ? 1 : (y > c.y ? -1 : 0));
        w.roads.push_back({nextId++, city.team, {x, y}, {nx, ny}, 1});
        x = nx;
        y = ny;
      }
    }
  }
}



void ensure_base_rail(World& w) {
  if (!w.railNodes.empty() || !w.railEdges.empty()) return;
  uint32_t nextNode = 1;
  uint32_t nextEdge = 1;
  for (const auto& c : w.cities) {
    w.railNodes.push_back({nextNode++, c.team, RailNodeType::Station, {(int)std::round(c.pos.x), (int)std::round(c.pos.y)}, 0, true});
  }
  for (const auto& b : w.buildings) {
    if (b.type != BuildingType::Market && b.type != BuildingType::Mine && b.type != BuildingType::CityCenter && b.type != BuildingType::Port) continue;
    RailNodeType rt = (b.type == BuildingType::Mine) ? RailNodeType::Depot : RailNodeType::Junction;
    w.railNodes.push_back({nextNode++, b.team, rt, {(int)std::round(b.pos.x), (int)std::round(b.pos.y)}, 0, true});
  }
  for (size_t i = 1; i < w.railNodes.size(); ++i) {
    auto& a = w.railNodes[i - 1];
    auto& b = w.railNodes[i];
    if (a.owner != b.owner) continue;
    w.railEdges.push_back({nextEdge++, a.owner, a.id, b.id, 1, false, false, false});
  }
}

void recompute_rail_networks(World& w) {
  for (auto& n : w.railNodes) n.networkId = 0;
  w.railNetworks.clear();
  uint32_t nextNet = 1;
  for (auto& start : w.railNodes) {
    if (!start.active || start.networkId != 0) continue;
    RailNetwork rn{};
    rn.id = nextNet++;
    rn.owner = start.owner;
    std::vector<uint32_t> stack{start.id};
    while (!stack.empty()) {
      uint32_t nid = stack.back(); stack.pop_back();
      auto it = std::find_if(w.railNodes.begin(), w.railNodes.end(), [&](const RailNode& n){ return n.id == nid; });
      if (it == w.railNodes.end() || it->networkId != 0 || !it->active || it->owner != rn.owner) continue;
      it->networkId = rn.id;
      ++rn.nodeCount;
      for (const auto& e : w.railEdges) {
        if (e.owner != rn.owner || e.disrupted) continue;
        if (e.aNode == nid) stack.push_back(e.bNode);
        if (e.bNode == nid) stack.push_back(e.aNode);
      }
    }
    for (const auto& e : w.railEdges) {
      if (e.owner != rn.owner || e.disrupted) continue;
      auto a = std::find_if(w.railNodes.begin(), w.railNodes.end(), [&](const RailNode& n){ return n.id == e.aNode; });
      auto b = std::find_if(w.railNodes.begin(), w.railNodes.end(), [&](const RailNode& n){ return n.id == e.bNode; });
      if (a != w.railNodes.end() && b != w.railNodes.end() && a->networkId == rn.id && b->networkId == rn.id) ++rn.edgeCount;
    }
    rn.active = rn.nodeCount > 1 && rn.edgeCount > 0;
    w.railNetworks.push_back(rn);
  }
}

std::vector<TrainRouteStep> route_between_nodes(const World& w, uint32_t owner, uint32_t fromNode, uint32_t toNode) {
  if (fromNode == 0 || toNode == 0 || fromNode == toNode) return {};
  std::vector<uint32_t> nodes;
  std::vector<int32_t> prevNode;
  std::vector<int32_t> prevEdge;
  nodes.reserve(w.railNodes.size());
  for (const auto& n : w.railNodes) if (n.owner == owner && n.active) nodes.push_back(n.id);
  prevNode.assign(nodes.size(), -1);
  prevEdge.assign(nodes.size(), -1);
  auto idx_of = [&](uint32_t id)->int { for (size_t i=0;i<nodes.size();++i) if (nodes[i]==id) return (int)i; return -1; };
  int sidx = idx_of(fromNode), didx = idx_of(toNode);
  if (sidx < 0 || didx < 0) return {};
  std::vector<int> q{ sidx };
  prevNode[sidx] = sidx;
  for (size_t qi = 0; qi < q.size(); ++qi) {
    int cur = q[qi];
    uint32_t nid = nodes[(size_t)cur];
    if (cur == didx) break;
    for (const auto& e : w.railEdges) {
      if (e.owner != owner || e.disrupted) continue;
      uint32_t other = 0;
      if (e.aNode == nid) other = e.bNode;
      else if (e.bNode == nid) other = e.aNode;
      if (other == 0) continue;
      int oi = idx_of(other);
      if (oi < 0 || prevNode[(size_t)oi] != -1) continue;
      prevNode[(size_t)oi] = cur;
      prevEdge[(size_t)oi] = (int32_t)e.id;
      q.push_back(oi);
    }
  }
  if (prevNode[(size_t)didx] == -1) return {};
  std::vector<TrainRouteStep> rev;
  for (int cur = didx; cur != sidx; cur = prevNode[(size_t)cur]) rev.push_back({(uint32_t)prevEdge[(size_t)cur], nodes[(size_t)cur]});
  std::reverse(rev.begin(), rev.end());
  return rev;
}

void ensure_trains(World& w) {
  if (!w.trains.empty()) return;
  uint32_t nextTrain = 1;
  for (const auto& n : w.railNodes) {
    if (n.type != RailNodeType::Station) continue;
    uint32_t dst = 0;
    for (const auto& o : w.railNodes) if (o.owner == n.owner && o.id != n.id && o.networkId == n.networkId) { dst = o.id; break; }
    if (!dst) continue;
    Train supply{};
    supply.id = nextTrain++; supply.owner = n.owner; supply.type = TrainType::Supply; supply.state = TrainState::Active; supply.currentNode = n.id; supply.destinationNode = dst; supply.capacity = 120.0f; supply.cargo = 70.0f; supply.cargoType = "Supply"; supply.route = route_between_nodes(w, n.owner, n.id, dst); supply.speed = 0.05f; supply.lastRouteTick = w.tick;
    if (!supply.route.empty()) w.trains.push_back(supply);
    Train freight = supply;
    freight.id = nextTrain++; freight.type = TrainType::Freight; freight.capacity = 160.0f; freight.cargo = 90.0f; freight.cargoType = "Freight"; freight.speed = 0.045f;
    if (!freight.route.empty()) w.trains.push_back(freight);
  }
}

void update_trains(World& w) {
  if (w.tick % 5 != 0) return;
  for (auto& t : w.trains) {
    if (t.state == TrainState::Inactive || t.route.empty()) continue;
    if (t.routeCursor >= t.route.size()) {
      std::swap(t.currentNode, t.destinationNode);
      t.route = route_between_nodes(w, t.owner, t.currentNode, t.destinationNode);
      t.routeCursor = 0;
      if (t.route.empty()) { t.state = TrainState::Delayed; continue; }
    }
    const auto step = t.route[t.routeCursor];
    auto eIt = std::find_if(w.railEdges.begin(), w.railEdges.end(), [&](const RailEdge& e){ return e.id == step.edgeId; });
    if (eIt == w.railEdges.end() || eIt->disrupted) { t.state = TrainState::Delayed; continue; }
    t.state = TrainState::Active;
    t.currentEdge = step.edgeId;
    t.segmentProgress = std::min(1.0f, t.segmentProgress + t.speed);
    if (t.segmentProgress >= 1.0f) {
      t.segmentProgress = 0.0f;
      t.currentNode = step.toNode;
      ++t.routeCursor;
      t.lastRouteTick = w.tick;
    }
  }
}

void apply_rail_logistics(World& w) {
  w.railThroughput = 0.0f;
  w.disruptedRailRoutes = 0;
  for (const auto& e : w.railEdges) if (e.disrupted) ++w.disruptedRailRoutes;
  for (const auto& t : w.trains) {
    if (t.state != TrainState::Active) continue;
    const float throughput = (t.cargo / std::max(1.0f, t.capacity)) * (t.type == TrainType::Freight ? 1.6f : 1.25f);
    w.railThroughput += throughput;
    if (t.owner < w.players.size()) {
      if (t.type == TrainType::Freight) {
        w.players[t.owner].resources[ridx(Resource::Metal)] += 0.012f * throughput;
        w.players[t.owner].resources[ridx(Resource::Wealth)] += 0.016f * throughput;
      } else if (t.type == TrainType::Supply) {
        w.players[t.owner].resources[ridx(Resource::Food)] += 0.01f * throughput;
      }
    }
  }
}

void recompute_trade_routes(World& w) {
  if (w.tick % 50 != 0 && !w.tradeRoutes.empty()) return;
  w.tradeRoutes.clear();
  uint32_t nextId = 1;
  for (size_t i = 0; i < w.cities.size(); ++i) {
    for (size_t j = i + 1; j < w.cities.size(); ++j) {
      const auto& a = w.cities[i];
      const auto& b = w.cities[j];
      if (!(a.team == b.team || trade_access_allowed(w, a.team, b.team))) continue;
      float d = dist(a.pos, b.pos);
      bool railBonus = false;
      for (const auto& n : w.railNodes) if (n.owner == a.team && n.type == RailNodeType::Station) { railBonus = true; break; }
      float roadBonus = (near_friendly_road(w, a.team, a.pos) && near_friendly_road(w, b.team, b.pos)) ? 1.3f : 1.0f;
      if (railBonus) roadBonus *= 1.2f;
      int ax = std::clamp((int)a.pos.x, 0, w.width - 1), ay = std::clamp((int)a.pos.y, 0, w.height - 1);
      int bx = std::clamp((int)b.pos.x, 0, w.width - 1), by = std::clamp((int)b.pos.y, 0, w.height - 1);
      bool contested = (w.territoryOwner[ay * w.width + ax] != a.team) || (w.territoryOwner[by * w.width + bx] != b.team);
      float eff = std::clamp((80.0f / std::max(25.0f, d)) * roadBonus * (contested ? 0.35f : 1.0f), 0.0f, 1.25f);
      bool active = eff > 0.2f;
      float wealth = active ? eff * 0.08f : 0.0f;
      w.tradeRoutes.push_back({nextId++, a.team, a.id, b.id, active, eff, wealth, w.tick});
      if (a.team != b.team) w.tradeRoutes.push_back({nextId++, b.team, b.id, a.id, active, eff, wealth, w.tick});
    }
  }
}

void apply_trade_income(World& w, float dt) {
  w.logisticsTradeActiveCount = 0;
  for (const auto& r : w.tradeRoutes) {
    if (!r.active || r.team >= w.players.size()) continue;
    w.players[r.team].resources[ridx(Resource::Wealth)] += r.wealthPerTick * dt * 20.0f;
    if (w.players[r.team].civilization.scienceBias > 1.1f) {
      w.players[r.team].resources[ridx(Resource::Knowledge)] += r.wealthPerTick * 0.2f * dt * 20.0f;
    }
    ++w.logisticsTradeActiveCount;
  }
}

void recompute_supply(World& w) {
  if (w.tick % 20 != 0) return;
  w.suppliedUnits = 0;
  w.lowSupplyUnits = 0;
  w.outOfSupplyUnits = 0;
  for (auto& u : w.units) {
    float best = 1e9f;
    for (const auto& c : w.cities) if (c.team == u.team) best = std::min(best, dist(u.pos, c.pos));
    for (const auto& b : w.buildings) if (b.team == u.team && (b.type == BuildingType::CityCenter || b.type == BuildingType::Market)) best = std::min(best, dist(u.pos, b.pos));
    if (near_friendly_road(w, u.team, u.pos)) best *= 0.8f;
    for (const auto& t : w.trains) if (t.owner == u.team && t.type == TrainType::Supply && t.state == TrainState::Active) { best *= 0.88f; break; }
    int tx = std::clamp((int)u.pos.x, 0, w.width - 1), ty = std::clamp((int)u.pos.y, 0, w.height - 1);
    if (w.territoryOwner[ty * w.width + tx] != u.team) best *= 1.2f;
    if (best < 24.0f) u.supplyState = SupplyState::InSupply;
    else if (best < 44.0f) u.supplyState = SupplyState::LowSupply;
    else u.supplyState = SupplyState::OutOfSupply;
    if (u.supplyState == SupplyState::InSupply) ++w.suppliedUnits;
    else if (u.supplyState == SupplyState::LowSupply) ++w.lowSupplyUnits;
    else ++w.outOfSupplyUnits;
  }
}

void apply_supply_effects(World& w, float dt) {
  for (auto& u : w.units) {
    if (u.supplyState == SupplyState::LowSupply) u.hp = std::max(1.0f, u.hp - 0.02f * dt * 20.0f);
    if (u.supplyState == SupplyState::OutOfSupply) u.hp -= 0.14f * dt * 20.0f;
  }
}

void update_operations(World& w) {
  if (w.tick % 80 != 0) return;
  w.operations.clear();
  w.railNodes.clear();
  w.railEdges.clear();
  w.railNetworks.clear();
  w.trains.clear();
  uint32_t nextId = 1;
  for (const auto& p : w.players) {
    if (!p.isCPU || !p.alive) continue;
    OperationType t = OperationType::RallyAndPush;
    if (p.civilization.defense > 1.1f) t = OperationType::DefendBorder;
    else if (p.civilization.economyBias > 1.1f) t = OperationType::SecureRoute;
    else if (p.civilization.aggression > 1.1f) t = OperationType::AssaultCity;
    if (p.civilization.economyBias > 1.15f) t = OperationType::NavalPatrol;
    if (p.civilization.aggression > 1.2f) t = OperationType::AmphibiousAssault;
    glm::vec2 target{(p.id == 0) ? 85.0f : 20.0f, (p.id == 0) ? 85.0f : 20.0f};
    for (const auto& c : w.cities) if (c.team != p.id && !players_allied(w, c.team, p.id)) { target = c.pos; break; }
    w.operations.push_back({nextId++, p.id, t, target, w.tick, true});
    ++w.logisticsOperationIssuedCount;
  }
}

int military_strength(const World& w, uint16_t team) {
  int s = 0;
  for (const auto& u : w.units) if (u.team == team && u.hp > 0 && u.type != UnitType::Worker) s += static_cast<int>(u.hp + u.attack * 10.0f);
  return s;
}

void update_world_tension(World& w) {
  if (w.tick % 20 != 0) return;
  float drift = -0.01f;
  int wars = 0;
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t j = i + 1; j < w.players.size(); ++j) if (players_at_war(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j))) ++wars;
  }
  drift += wars * 0.03f;
  if (w.navalCombatEvents > 0 || w.combatEngagementCount > 0) drift += 0.02f;
  w.worldTension = std::clamp(w.worldTension + drift, 0.0f, 100.0f);
}

void update_espionage(World& w) {
  if (w.tick % 100 == 0) {
    uint32_t nextId = 1;
    for (const auto& op : w.espionageOps) nextId = std::max(nextId, op.id + 1);
    for (const auto& p : w.players) {
      if (!p.isCPU || !p.alive) continue;
      uint16_t target = p.id;
      for (const auto& e : w.players) if (e.id != p.id && e.alive && !players_allied(w, p.id, e.id)) { target = e.id; break; }
      if (target == p.id) continue;
      int activeByTeam = 0;
      for (const auto& op : w.espionageOps) if (op.actor == p.id && op.state == EspionageOpState::Active) ++activeByTeam;
      if (activeByTeam >= 1) continue;
      EspionageOpType type = EspionageOpType::ReconCity;
      if (w.worldTension > 45.0f) type = EspionageOpType::SabotageSupply;
      else if (p.civilization.scienceBias > 1.05f) type = EspionageOpType::RevealRoute;
      else if (p.civilization.aggression > 1.05f) type = EspionageOpType::SabotageEconomy;
      w.espionageOps.push_back({nextId++, p.id, target, type, w.tick, 140u, EspionageOpState::Active, 0});
    }
  }

  for (auto& op : w.espionageOps) {
    if (op.state != EspionageOpState::Active) continue;
    if (w.tick < op.startTick + op.durationTicks) continue;
    const auto counter = std::count_if(w.espionageOps.begin(), w.espionageOps.end(), [&](const EspionageOp& e){ return e.target == op.target && e.type == EspionageOpType::CounterIntel && e.state == EspionageOpState::Active; });
    const int score = (int)((w.tick + op.actor * 17 + op.target * 31 + (uint16_t)op.type * 13) % 100);
    const bool detected = score < (20 + (int)counter * 15);
    if (detected) {
      op.state = EspionageOpState::Failed;
      ++w.diplomacyEventCount;
      emit_event(w, GameplayEventType::EspionageFailure, op.actor, op.target, op.id, "espionage_failed");
      w.worldTension = std::min(100.0f, w.worldTension + 1.0f);
      continue;
    }
    op.state = EspionageOpState::Completed;
    ++w.diplomacyEventCount;
    emit_event(w, GameplayEventType::EspionageSuccess, op.actor, op.target, op.id, "espionage_success");
    if (op.type == EspionageOpType::ReconCity || op.type == EspionageOpType::RevealRoute) {
      for (const auto& c : w.cities) if (c.team == op.target) {
        const int minX = std::max(0, (int)std::floor(c.pos.x) - 4), maxX = std::min(w.width - 1, (int)std::floor(c.pos.x) + 4);
        const int minY = std::max(0, (int)std::floor(c.pos.y) - 4), maxY = std::min(w.height - 1, (int)std::floor(c.pos.y) + 4);
        for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) w.fog[y * w.width + x] = 0;
        break;
      }
    } else if (op.type == EspionageOpType::SabotageEconomy) {
      if (op.target < w.players.size()) w.players[op.target].resources[ridx(Resource::Wealth)] = std::max(0.0f, w.players[op.target].resources[ridx(Resource::Wealth)] - 30.0f);
    } else if (op.type == EspionageOpType::SabotageSupply) {
      for (auto& u : w.units) if (u.team == op.target && u.supplyState == SupplyState::InSupply) { u.supplyState = SupplyState::LowSupply; break; }
    }
    w.worldTension = std::min(100.0f, w.worldTension + 2.0f);
    if (relation_of(w, op.actor, op.target) == DiplomacyRelation::Neutral && op.type != EspionageOpType::ReconCity) {
      set_relation(w, op.actor, op.target, DiplomacyRelation::Ceasefire);
    }
  }
}

void update_ai_diplomacy(World& w) {
  if (w.tick % 60 != 0) return;
  for (auto& p : w.players) {
    if (!p.isCPU || !p.alive) continue;
    uint16_t bestEnemy = p.id;
    int bestEnemyStr = -1;
    int ownStr = military_strength(w, p.id);
    for (const auto& e : w.players) {
      if (e.id == p.id || !e.alive) continue;
      if (players_allied(w, p.id, e.id)) continue;
      int es = military_strength(w, e.id);
      if (es > bestEnemyStr) { bestEnemyStr = es; bestEnemy = e.id; }
    }
    StrategicPosture next = StrategicPosture::Defensive;
    if (w.worldTension > 70.0f || (bestEnemy != p.id && players_at_war(w, p.id, bestEnemy))) next = StrategicPosture::TotalWar;
    else if (w.worldTension > 45.0f || p.civilization.aggression > 1.15f) next = StrategicPosture::Escalating;
    else if (p.civilization.economyBias > p.civilization.militaryBias) next = StrategicPosture::TradeFocused;
    else if (ownStr > std::max(1, bestEnemyStr)) next = StrategicPosture::Expansionist;
    if (p.id < w.strategicPosture.size() && w.strategicPosture[p.id] != next) {
      w.strategicPosture[p.id] = next;
      ++w.postureChangeCount;
      ++w.diplomacyEventCount;
      emit_event(w, GameplayEventType::PostureChanged, p.id, p.id, (uint32_t)next, posture_name(next));
    }

    if (bestEnemy != p.id && !players_at_war(w, p.id, bestEnemy) && next == StrategicPosture::TotalWar) declare_war(w, p.id, bestEnemy);
    if (bestEnemy != p.id && relation_of(w, p.id, bestEnemy) == DiplomacyRelation::Neutral && next == StrategicPosture::TradeFocused) establish_trade_agreement(w, p.id, bestEnemy);
    if (bestEnemy != p.id && relation_of(w, p.id, bestEnemy) == DiplomacyRelation::Neutral && next == StrategicPosture::Expansionist && ownStr > bestEnemyStr * 13 / 10) declare_war(w, p.id, bestEnemy);
  }
}

} // namespace


const char* world_preset_name(WorldPreset preset) {
  switch (preset) {
    case WorldPreset::Pangaea: return "pangaea";
    case WorldPreset::Continents: return "continents";
    case WorldPreset::Archipelago: return "archipelago";
    case WorldPreset::InlandSea: return "inland_sea";
    case WorldPreset::MountainWorld: return "mountain_world";
  }
  return "pangaea";
}

WorldPreset parse_world_preset(const std::string& value) {
  if (value == "continents") return WorldPreset::Continents;
  if (value == "archipelago") return WorldPreset::Archipelago;
  if (value == "inland_sea") return WorldPreset::InlandSea;
  if (value == "mountain_world") return WorldPreset::MountainWorld;
  return WorldPreset::Pangaea;
}

void set_world_preset(World& world, WorldPreset preset) { world.worldPreset = preset; }

bool gameplay_orders_allowed(const World& world) { return world.match.phase == MatchPhase::Running; }
void set_match_phase(World& world, MatchPhase phase) { world.match.phase = phase; world.gameOver = phase != MatchPhase::Running; }

void consume_replay_commands(std::vector<ReplayCommand>& out) { out = std::move(gReplayCommands); gReplayCommands.clear(); }

void consume_gameplay_events(std::vector<GameplayEvent>& out) { out = std::move(gGameplayEvents); gGameplayEvents.clear(); }

bool is_unit_visible_to_player(const World& world, const Unit& unit, uint16_t playerId) {
  if (playerId >= world.players.size()) return false;
  if (playerId == unit.team || players_allied(world, playerId, unit.team)) return true;
  if (world.godMode) return true;
  const int gx = std::clamp(static_cast<int>(unit.pos.x), 0, world.width - 1);
  const int gy = std::clamp(static_cast<int>(unit.pos.y), 0, world.height - 1);
  const int tile = gy * world.width + gx;
  const size_t base = static_cast<size_t>(playerId) * static_cast<size_t>(world.width * world.height);
  if (base + static_cast<size_t>(tile) >= world.fogVisibilityByPlayer.size()) return false;
  const bool inVision = world.fogVisibilityByPlayer[base + static_cast<size_t>(tile)] > 0;
  if (!inVision) return false;
  if (!unit_has_stealth(unit)) return true;
  if (unit.stealthRevealTicks > 0) return true;
  for (const auto& own : world.units) {
    if (own.hp <= 0.0f || own.team != playerId || own.embarked) continue;
    const float det = unit_detection_radius(own);
    if (det <= 0.0f) continue;
    if (dist(own.pos, unit.pos) <= det) return true;
  }
  for (const auto& b : world.buildings) {
    if (b.hp <= 0.0f || b.underConstruction || b.team != playerId) continue;
    float det = 0.0f;
    if (b.type == BuildingType::Library) det = 14.0f;
    else if (b.type == BuildingType::Port) det = 11.0f;
    if (det > 0.0f && dist(b.pos, unit.pos) <= det) return true;
  }
  return false;
}

bool players_allied(const World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size()) return a == b;
  if (a == b) return true;
  if (world.players[a].teamId == world.players[b].teamId) return true;
  auto rel = relation_of(world, a, b);
  return rel == DiplomacyRelation::Allied || rel == DiplomacyRelation::Ceasefire;
}

bool players_at_war(const World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size() || a == b) return false;
  return relation_of(world, a, b) == DiplomacyRelation::War;
}

bool trade_access_allowed(const World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size()) return false;
  if (a == b || players_allied(world, a, b)) return true;
  const auto treaty = treaty_of(world, a, b);
  return treaty.tradeAgreement || treaty.openBorders;
}

bool declare_war(World& world, uint16_t actor, uint16_t target) {
  if (actor >= world.players.size() || target >= world.players.size() || actor == target) return false;
  if (players_at_war(world, actor, target)) return false;
  auto treaty = treaty_of(world, actor, target);
  treaty.alliance = false;
  treaty.tradeAgreement = false;
  treaty.openBorders = false;
  treaty.nonAggression = false;
  treaty.lastChangedTick = world.tick;
  set_treaty(world, actor, target, treaty);
  set_relation(world, actor, target, DiplomacyRelation::War);
  world.worldTension = std::min(100.0f, world.worldTension + 8.0f);
  ++world.diplomacyEventCount;
  emit_event(world, GameplayEventType::WarDeclared, actor, target, 0, "war_declared");
  return true;
}

bool form_alliance(World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size() || a == b) return false;
  auto treaty = treaty_of(world, a, b);
  treaty.alliance = true;
  treaty.tradeAgreement = true;
  treaty.openBorders = true;
  treaty.lastChangedTick = world.tick;
  set_treaty(world, a, b, treaty);
  set_relation(world, a, b, DiplomacyRelation::Allied);
  ++world.diplomacyEventCount;
  emit_event(world, GameplayEventType::AllianceFormed, a, b, 0, "alliance_formed");
  return true;
}

bool establish_trade_agreement(World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size() || a == b) return false;
  if (players_at_war(world, a, b)) return false;
  auto treaty = treaty_of(world, a, b);
  treaty.tradeAgreement = true;
  treaty.openBorders = true;
  treaty.lastChangedTick = world.tick;
  set_treaty(world, a, b, treaty);
  ++world.diplomacyEventCount;
  emit_event(world, GameplayEventType::TradeAgreementCreated, a, b, 0, "trade_agreement_created");
  return true;
}

bool break_treaty(World& world, uint16_t a, uint16_t b) {
  if (a >= world.players.size() || b >= world.players.size() || a == b) return false;
  const auto prev = treaty_of(world, a, b);
  DiplomacyTreaty treaty{};
  treaty.lastChangedTick = world.tick;
  set_treaty(world, a, b, treaty);
  if (prev.alliance) {
    ++world.diplomacyEventCount;
    emit_event(world, GameplayEventType::AllianceBroken, a, b, 0, "alliance_broken");
  }
  if (prev.tradeAgreement) {
    ++world.diplomacyEventCount;
    emit_event(world, GameplayEventType::TradeAgreementBroken, a, b, 0, "trade_agreement_broken");
  }
  if (!players_at_war(world, a, b)) set_relation(world, a, b, DiplomacyRelation::Neutral);
  return true;
}

const char* train_type_name(TrainType t) {
  if (t == TrainType::Freight) return "Freight";
  if (t == TrainType::Armored) return "Armored";
  return "Supply";
}

TrainType parse_train_type(const std::string& v) {
  if (v == "Freight") return TrainType::Freight;
  if (v == "Armored") return TrainType::Armored;
  return TrainType::Supply;
}

RailNodeType parse_rail_node_type(const std::string& v) {
  if (v == "Station") return RailNodeType::Station;
  if (v == "Depot") return RailNodeType::Depot;
  return RailNodeType::Junction;
}

const char* rail_node_type_name(RailNodeType t) {
  if (t == RailNodeType::Station) return "Station";
  if (t == RailNodeType::Depot) return "Depot";
  return "Junction";
}

const char* posture_name(StrategicPosture posture) {
  switch (posture) {
    case StrategicPosture::Expansionist: return "EXPANSIONIST";
    case StrategicPosture::Defensive: return "DEFENSIVE";
    case StrategicPosture::TradeFocused: return "TRADE_FOCUSED";
    case StrategicPosture::Escalating: return "ESCALATING";
    case StrategicPosture::TotalWar: return "TOTAL_WAR";
  }
  return "DEFENSIVE";
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
  w.mission = {};
  w.missionRuntime = {};
  w.guardianSites.clear();
  w.guardiansDiscovered = 0;
  w.guardiansSpawned = 0;
  w.guardiansJoined = 0;
  w.guardiansKilled = 0;
  w.hostileGuardianEvents = 0;
  w.alliedGuardianEvents = 0;
  w.roads.clear();
  w.tradeRoutes.clear();
  w.operations.clear();
  w.railNodes.clear();
  w.railEdges.clear();
  w.railNetworks.clear();
  w.trains.clear();
  w.diplomacy.assign(w.players.size() * w.players.size(), DiplomacyRelation::Neutral);
  w.treaties.assign(w.players.size() * w.players.size(), DiplomacyTreaty{});
  w.strategicPosture.assign(w.players.size(), StrategicPosture::Defensive);
  w.espionageOps.clear();
  w.airUnits.clear();
  w.detectors.clear();
  w.strategicStrikes.clear();
  w.denialZones.clear();
  w.radarContactByPlayer.clear();
  w.worldTension = 0.0f;
  for (size_t i = 0; i < w.players.size(); ++i) {
    set_relation(w, static_cast<uint16_t>(i), static_cast<uint16_t>(i), DiplomacyRelation::Allied);
  }
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t j = i + 1; j < w.players.size(); ++j) {
      if (w.players[i].teamId == w.players[j].teamId) {
        DiplomacyTreaty t{}; t.alliance = true; t.tradeAgreement = true; t.openBorders = true;
        set_treaty(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j), t);
        set_relation(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j), DiplomacyRelation::Allied);
      }
    }
  }
  w.logisticsRoadCount = 0;
  w.logisticsTradeActiveCount = 0;
  w.logisticsOperationIssuedCount = 0;
  w.railNodeCount = 0;
  w.railEdgeCount = 0;
  w.activeRailNetworks = 0;
  w.activeTrains = 0;
  w.activeSupplyTrains = 0;
  w.activeFreightTrains = 0;
  w.railThroughput = 0.0f;
  w.disruptedRailRoutes = 0;
  w.diplomacyEventCount = 0;
  w.postureChangeCount = 0;
  w.suppliedUnits = 0;
  w.lowSupplyUnits = 0;
  w.outOfSupplyUnits = 0;
  gReplayCommands.clear();
}

bool validate_size(const World& w, const std::vector<float>& v) { return (int)v.size() == w.width * w.height; }
bool validate_size_u8(const World& w, const std::vector<uint8_t>& v) { return (int)v.size() == w.width * w.height; }
bool validate_size_i32(const World& w, const std::vector<int32_t>& v) { return (int)v.size() == w.width * w.height; }

void eval_triggers(World& w) {
  std::sort(w.triggers.begin(), w.triggers.end(), [](const Trigger& a, const Trigger& b){ return a.id < b.id; });
  auto apply_objective_state = [&](uint32_t oid, ObjectiveState st, uint16_t actor) {
    for (auto& o : w.objectives) if (o.id == oid) {
      if (o.state != st) {
        ++w.objectiveStateChangeCount;
        if (st == ObjectiveState::Completed) emit_event(w, GameplayEventType::ObjectiveCompleted, actor, o.owner, o.id, o.title);
      }
      o.state = st;
    }
  };
  for (auto& t : w.triggers) {
    if (t.once && t.fired) continue;
    bool hit = false;
    switch (t.condition.type) {
      case TriggerType::TickReached: hit = w.tick >= t.condition.tick; break;
      case TriggerType::UnitDestroyed: {
        bool unitAlive = false; for (const auto& u : w.units) if (u.id == t.condition.entityId && u.hp > 0) unitAlive = true;
        hit = !unitAlive;
      } break;
      case TriggerType::BuildingDestroyed: {
        bool buildingAlive = false; for (const auto& b : w.buildings) if (b.id == t.condition.entityId && b.hp > 0) buildingAlive = true;
        hit = !buildingAlive;
      } break;
      case TriggerType::BuildingCompleted: {
        for (const auto& b : w.buildings) if ((t.condition.player == UINT16_MAX || b.team == t.condition.player) && b.type == t.condition.buildingType && !b.underConstruction) { hit = true; break; }
      } break;
      case TriggerType::ObjectiveCompleted: for (const auto& o : w.objectives) if (o.id == t.condition.objectiveId && o.state == ObjectiveState::Completed) hit = true; break;
      case TriggerType::ObjectiveFailed: for (const auto& o : w.objectives) if (o.id == t.condition.objectiveId && o.state == ObjectiveState::Failed) hit = true; break;
      case TriggerType::PlayerEliminated: if (t.condition.player < w.players.size()) hit = !w.players[t.condition.player].alive; break;
      case TriggerType::AreaEntered: {
        auto it = std::find_if(w.triggerAreas.begin(), w.triggerAreas.end(), [&](const TriggerArea& a){ return a.id == t.condition.areaId; });
        if (it != w.triggerAreas.end()) for (const auto& u : w.units) if ((t.condition.player == UINT16_MAX || u.team == t.condition.player) && u.pos.x >= it->min.x && u.pos.x <= it->max.x && u.pos.y >= it->min.y && u.pos.y <= it->max.y) { hit = true; break; }
      } break;
      case TriggerType::DiplomacyChanged:
        if (t.condition.player < w.players.size() && t.condition.playerB < w.players.size()) hit = relation_of(w, t.condition.player, t.condition.playerB) == t.condition.diplomacy;
        break;
      case TriggerType::WorldTensionReached: hit = w.worldTension >= t.condition.worldTension; break;
      case TriggerType::StrategicStrikeLaunched: hit = w.strategicStrikeEvents > 0; break;
      case TriggerType::WonderCompleted: for (const auto& b : w.buildings) if (!b.underConstruction && b.type == BuildingType::Wonder) { hit = true; break; } break;
      case TriggerType::CargoLanded: hit = w.disembarkEvents > 0; break;
    }
    if (!hit) continue;
    t.fired = true;
    ++w.triggerExecutionCount;
    ++w.missionRuntime.firedTriggerCount;
    for (const auto& a : t.actions) {
      ++w.missionRuntime.scriptedActionCount;
      if (a.type == TriggerActionType::ShowMessage) w.objectiveLog.push_back({w.tick, a.text});
      else if (a.type == TriggerActionType::ActivateObjective) apply_objective_state(a.objectiveId, ObjectiveState::Active, a.player);
      else if (a.type == TriggerActionType::CompleteObjective) apply_objective_state(a.objectiveId, ObjectiveState::Completed, a.player);
      else if (a.type == TriggerActionType::FailObjective) apply_objective_state(a.objectiveId, ObjectiveState::Failed, a.player);
      else if (a.type == TriggerActionType::GrantResources) { if (a.player < w.players.size()) for (size_t i = 0; i < a.resources.size(); ++i) w.players[a.player].resources[i] += a.resources[i]; }
      else if (a.type == TriggerActionType::SpawnUnits) { for (uint32_t i = 0; i < a.spawnCount; ++i) spawn_unit(w, a.player, a.spawnUnitType, {a.spawnPos.x + 0.7f * i, a.spawnPos.y}); }
      else if (a.type == TriggerActionType::SpawnBuildings) {
        for (uint32_t i = 0; i < a.spawnCount; ++i) {
          uint32_t bid = 1; for (const auto& b : w.buildings) bid = std::max(bid, b.id + 1);
          Building bb{}; bb.id = bid; bb.team = a.player; bb.type = a.spawnBuildingType; bb.pos = {a.spawnPos.x + 2.0f * i, a.spawnPos.y}; bb.size = gBuildDefs[bidx(bb.type)].size; bb.underConstruction = false; bb.buildProgress = 1.0f; bb.buildTime = gBuildDefs[bidx(bb.type)].buildTime; bb.maxHp = (bb.type==BuildingType::CityCenter?2200.0f:1000.0f); bb.hp = bb.maxHp;
          w.buildings.push_back(bb);
        }
      }
      else if (a.type == TriggerActionType::ChangeDiplomacy) set_relation(w, a.player, a.playerB, a.diplomacy);
      else if (a.type == TriggerActionType::SetWorldTension) w.worldTension = a.worldTension;
      else if (a.type == TriggerActionType::RevealArea) {
        auto it = std::find_if(w.triggerAreas.begin(), w.triggerAreas.end(), [&](const TriggerArea& ar){ return ar.id == a.areaId; });
        if (it != w.triggerAreas.end()) {
          int minX = std::max(0, (int)std::floor(it->min.x)); int maxX = std::min(w.width-1, (int)std::ceil(it->max.x));
          int minY = std::max(0, (int)std::floor(it->min.y)); int maxY = std::min(w.height-1, (int)std::ceil(it->max.y));
          for (int y=minY;y<=maxY;++y) for (int x=minX;x<=maxX;++x) w.fog[y*w.width+x]=0;
        }
      } else if (a.type == TriggerActionType::LaunchOperation) {
        w.operations.push_back({w.operations.empty()?1u:w.operations.back().id+1,a.player,a.operationType,a.operationTarget,w.tick,true});
      } else if (a.type == TriggerActionType::EndMatchVictory) {
        w.missionRuntime.status = MissionStatus::Victory; w.missionRuntime.resultTag = w.mission.victoryOutcomeTag; apply_match_end(w, VictoryCondition::Conquest, a.winner, false);
      } else if (a.type == TriggerActionType::EndMatchDefeat) {
        w.missionRuntime.status = MissionStatus::Defeat; w.missionRuntime.resultTag = w.mission.defeatOutcomeTag; apply_match_end(w, VictoryCondition::Conquest, a.winner, false);
      } else if (a.type == TriggerActionType::RunLuaHook) run_lua_hook(w, a.luaHook);
    }
  }
  w.missionRuntime.activeObjectives.clear();
  for (const auto& o : w.objectives) if (o.state == ObjectiveState::Active) w.missionRuntime.activeObjectives.push_back(o.id);
}

void set_worker_threads(int threads) { gWorkerThreads = std::max(1, threads); }
int worker_threads() { return gWorkerThreads; }

void run_task_graph(TaskGraph& graph) {
  const size_t count = graph.jobs.size();
  if (count == 0) return;
  const int workers = std::min<int>(std::max(1, gWorkerThreads), static_cast<int>(count));
  if (workers <= 1) { for (auto& j : graph.jobs) if (j.execute) j.execute(); return; }
  std::vector<std::thread> pool;
  pool.reserve(static_cast<size_t>(workers));
  std::atomic<size_t> next{0};
  for (int i = 0; i < workers; ++i) {
    pool.emplace_back([&]() {
      while (true) {
        size_t idx = next.fetch_add(1, std::memory_order_relaxed);
        if (idx >= count) break;
        auto& job = graph.jobs[idx];
        if (job.execute) job.execute();
      }
    });
  }
  for (auto& t : pool) t.join();
}

void initialize_world(World& w, uint32_t seed) {

  load_defs_once();
  load_guardian_defs(w);
  gNav.cache.clear();
  gPendingNavRequests.clear();
  gCompletedNavResults.clear();
  gNextNavRequestId = 1;
  gNav.nextMoveOrder = 1;
  gSpatial.cells.clear();
  w.navVersion = 1;
  w.seed = seed;
  w.heightmap.resize(w.width * w.height);
  w.fertility.resize(w.width * w.height);
  w.territoryOwner.resize(w.width * w.height);
  w.fog.assign(w.width * w.height, 0);
  w.fogVisibilityByPlayer.clear();
  w.fogExploredByPlayer.clear();
  w.fogMaskByPlayer.clear();
  generate_macro_landmass(w);
  apply_tectonics(w);
  rebuild_terrain_classes(w);
  classify_coast_and_landmass(w);
  for (int i = 0; i < w.width * w.height; ++i) {
    float coastBonus = (!w.coastClassMap.empty() && w.coastClassMap[(size_t)i] > 0) ? 0.15f : 0.0f;
    w.fertility[(size_t)i] = std::clamp(0.85f - std::abs(w.heightmap[(size_t)i]) + coastBonus, 0.05f, 1.0f);
  }
  assign_biomes(w);
  generate_hydrology(w);
  build_resource_geography(w);
  build_start_candidates(w);
  build_mythic_candidates(w);
  spawn_biome_resources(w);

  apply_world_defaults(w);
  w.roads.clear();
  w.triggerAreas.clear();
  w.objectives.clear();
  w.triggers.clear();
  w.units.clear();
  w.cities.clear();
  w.buildings.clear();
  w.resourceNodes.clear();
  for (int i = 0; i < 6; ++i) {
    spawn_unit(w, 0, UnitType::Worker, {18.0f + i * 0.8f, 24.0f});
    spawn_unit(w, 1, UnitType::Worker, {92.0f + i * 0.8f, 89.0f});
  }
  for (int i = 0; i < 8; ++i) {
    spawn_unit(w, 0, UnitType::Infantry, {17.0f + i * 0.8f, 22.0f});
    spawn_unit(w, 1, UnitType::Infantry, {91.0f + i * 0.8f, 87.0f});
  }
  w.cities.push_back({1, 0, {18.0f, 18.0f}, 2, true});
  w.cities.push_back({2, 1, {92.0f, 92.0f}, 2, true});
  Building c0{}; c0.id = 1; c0.team = 0; c0.type = BuildingType::CityCenter; c0.pos = {18.0f, 18.0f}; c0.size = gBuildDefs[bidx(c0.type)].size; c0.underConstruction = false; c0.buildProgress = 1.0f; c0.buildTime = gBuildDefs[bidx(c0.type)].buildTime; c0.maxHp = 2200.0f; c0.hp = c0.maxHp; w.buildings.push_back(c0);
  Building c1{}; c1.id = 2; c1.team = 1; c1.type = BuildingType::CityCenter; c1.pos = {92.0f, 92.0f}; c1.size = gBuildDefs[bidx(c1.type)].size; c1.underConstruction = false; c1.buildProgress = 1.0f; c1.buildTime = gBuildDefs[bidx(c1.type)].buildTime; c1.maxHp = 2200.0f; c1.hp = c1.maxHp; w.buildings.push_back(c1);
  w.mission = {};
  w.missionRuntime = {};
  w.guardianSites.clear();
  generate_guardian_sites(w);
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
  w.worldPreset = parse_world_preset(j.value("worldPreset", std::string("pangaea")));
  initialize_world(w, w.seed);
  if (j.contains("heightmap")) { w.heightmap = j["heightmap"].get<std::vector<float>>(); if (!validate_size(w, w.heightmap)) { err = "heightmap size mismatch"; return false; } }
  if (j.contains("fertility")) { w.fertility = j["fertility"].get<std::vector<float>>(); if (!validate_size(w, w.fertility)) { err = "fertility size mismatch"; return false; } }
  if (j.contains("terrainOverrides")) {
    auto to = j["terrainOverrides"];
    if (to.contains("height") && to["height"].is_array()) { w.heightmap = to["height"].get<std::vector<float>>(); if (!validate_size(w, w.heightmap)) { err = "terrainOverrides.height size mismatch"; return false; } }
    if (to.contains("fertility") && to["fertility"].is_array()) { w.fertility = to["fertility"].get<std::vector<float>>(); if (!validate_size(w, w.fertility)) { err = "terrainOverrides.fertility size mismatch"; return false; } }
  }
  rebuild_terrain_classes(w);
  classify_coast_and_landmass(w);
  assign_biomes(w);
  generate_hydrology(w);
  build_resource_geography(w);
  build_start_candidates(w);
  build_mythic_candidates(w);
  if (j.contains("biomeMap")) {
    auto bm = j["biomeMap"].get<std::vector<uint8_t>>();
    if (!validate_size_u8(w, bm)) { err = "biomeMap size mismatch"; return false; }
    w.biomeMap = std::move(bm);
  }
  if (j.contains("temperatureMap")) {
    auto tm = j["temperatureMap"].get<std::vector<float>>();
    if (!validate_size(w, tm)) { err = "temperatureMap size mismatch"; return false; }
    w.temperatureMap = std::move(tm);
  }
  if (j.contains("moistureMap")) {
    auto mm = j["moistureMap"].get<std::vector<float>>();
    if (!validate_size(w, mm)) { err = "moistureMap size mismatch"; return false; }
    w.moistureMap = std::move(mm);
  }
  if (j.contains("coastClassMap")) {
    auto cm = j["coastClassMap"].get<std::vector<uint8_t>>();
    if (!validate_size_u8(w, cm)) { err = "coastClassMap size mismatch"; return false; }
    w.coastClassMap = std::move(cm);
  }
  if (j.contains("landmassIdByCell")) {
    auto lm = j["landmassIdByCell"].get<std::vector<int32_t>>();
    if (!validate_size_i32(w, lm)) { err = "landmassIdByCell size mismatch"; return false; }
    w.landmassIdByCell = std::move(lm);
  }
  if (j.contains("riverMap")) {
    auto rm = j["riverMap"].get<std::vector<uint8_t>>();
    if (!validate_size_u8(w, rm)) { err = "riverMap size mismatch"; return false; }
    w.riverMap = std::move(rm);
  }
  if (j.contains("lakeMap")) {
    auto lm = j["lakeMap"].get<std::vector<uint8_t>>();
    if (!validate_size_u8(w, lm)) { err = "lakeMap size mismatch"; return false; }
    w.lakeMap = std::move(lm);
  }
  if (j.contains("resourceWeightMap")) {
    auto rw = j["resourceWeightMap"].get<std::vector<float>>();
    if (!validate_size(w, rw)) { err = "resourceWeightMap size mismatch"; return false; }
    w.resourceWeightMap = std::move(rw);
  }
  if (j.contains("startCandidates") && j["startCandidates"].is_array()) {
    w.startCandidates.clear();
    for (const auto& sc : j["startCandidates"]) {
      StartCandidate c{};
      c.cell = sc.value("cell", -1);
      c.score = sc.value("score", 0.0f);
      c.civBiasMask = sc.value("civBiasMask", static_cast<uint8_t>(0));
      w.startCandidates.push_back(c);
    }
  }
  if (j.contains("mythicCandidates") && j["mythicCandidates"].is_array()) {
    w.mythicCandidates.clear();
    for (const auto& mc : j["mythicCandidates"]) {
      MythicCandidate c{};
      c.siteType = static_cast<GuardianSiteType>(mc.value("siteType", 0));
      c.cell = mc.value("cell", -1);
      c.score = mc.value("score", 0.0f);
      w.mythicCandidates.push_back(c);
    }
  }
  if (j.contains("waterMask")) {
    auto m = j["waterMask"].get<std::vector<uint8_t>>();
    if (!validate_size_u8(w, m)) { err = "waterMask size mismatch"; return false; }
    w.terrainClass = std::move(m);
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
  w.diplomacy.assign(w.players.size() * w.players.size(), DiplomacyRelation::Neutral);
  w.treaties.assign(w.players.size() * w.players.size(), DiplomacyTreaty{});
  w.strategicPosture.assign(w.players.size(), StrategicPosture::Defensive);
  for (size_t i = 0; i < w.players.size(); ++i) set_relation(w, static_cast<uint16_t>(i), static_cast<uint16_t>(i), DiplomacyRelation::Allied);
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t j = i + 1; j < w.players.size(); ++j) {
      if (w.players[i].teamId == w.players[j].teamId) {
        DiplomacyTreaty t{}; t.alliance = true; t.tradeAgreement = true; t.openBorders = true;
        set_treaty(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j), t);
        set_relation(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j), DiplomacyRelation::Allied);
      }
    }
  }
  w.worldTension = j.value("worldTension", 0.0f);
  if (j.contains("diplomacyRelations") && j["diplomacyRelations"].is_array()) {
    for (const auto& e : j["diplomacyRelations"]) {
      uint16_t a = e.value("a", (uint16_t)0), b = e.value("b", (uint16_t)0);
      std::string rel = e.value("relation", std::string("Neutral"));
      DiplomacyRelation r = DiplomacyRelation::Neutral;
      if (rel == "Allied") r = DiplomacyRelation::Allied;
      else if (rel == "War") r = DiplomacyRelation::War;
      else if (rel == "Ceasefire") r = DiplomacyRelation::Ceasefire;
      set_relation(w, a, b, r);
    }
  }
  if (j.contains("treaties") && j["treaties"].is_array()) {
    for (const auto& e : j["treaties"]) {
      uint16_t a = e.value("a", (uint16_t)0), b = e.value("b", (uint16_t)0);
      DiplomacyTreaty t{};
      t.tradeAgreement = e.value("tradeAgreement", false);
      t.openBorders = e.value("openBorders", false);
      t.alliance = e.value("alliance", false);
      t.nonAggression = e.value("nonAggression", false);
      t.lastChangedTick = e.value("lastChangedTick", 0u);
      set_treaty(w, a, b, t);
    }
  }
  if (j.contains("strategicPosture") && j["strategicPosture"].is_array()) {
    for (const auto& e : j["strategicPosture"]) {
      uint16_t p = e.value("player", (uint16_t)0);
      if (p >= w.strategicPosture.size()) continue;
      std::string s = e.value("posture", std::string("DEFENSIVE"));
      if (s == "EXPANSIONIST") w.strategicPosture[p] = StrategicPosture::Expansionist;
      else if (s == "TRADE_FOCUSED") w.strategicPosture[p] = StrategicPosture::TradeFocused;
      else if (s == "ESCALATING") w.strategicPosture[p] = StrategicPosture::Escalating;
      else if (s == "TOTAL_WAR") w.strategicPosture[p] = StrategicPosture::TotalWar;
      else w.strategicPosture[p] = StrategicPosture::Defensive;
    }
  }
  w.espionageOps.clear();
  if (j.contains("espionageOps") && j["espionageOps"].is_array()) {
    for (const auto& e : j["espionageOps"]) {
      EspionageOp op{};
      op.id = e.value("id", 0u); op.actor = e.value("actor", (uint16_t)0); op.target = e.value("target", (uint16_t)0);
      const std::string type = e.value("type", std::string("RECON_CITY"));
      if (type == "REVEAL_ROUTE") op.type = EspionageOpType::RevealRoute;
      else if (type == "SABOTAGE_ECONOMY") op.type = EspionageOpType::SabotageEconomy;
      else if (type == "SABOTAGE_SUPPLY") op.type = EspionageOpType::SabotageSupply;
      else if (type == "COUNTERINTEL") op.type = EspionageOpType::CounterIntel;
      op.startTick = e.value("startTick", 0u); op.durationTicks = e.value("durationTicks", 120u);
      const std::string state = e.value("state", std::string("ACTIVE"));
      if (state == "COMPLETED") op.state = EspionageOpState::Completed;
      else if (state == "FAILED") op.state = EspionageOpState::Failed;
      op.effectStrength = e.value("effectStrength", 0);
      w.espionageOps.push_back(op);
    }
  }
  w.cities.clear();
  if (j.contains("cities")) for (const auto& c : j["cities"]) { City cc{}; cc.id=c.value("id",0u); cc.team=c.value("team",0u); cc.pos={c["pos"][0].get<float>(), c["pos"][1].get<float>()}; cc.level=c.value("level",1); cc.capital=c.value("capital",false); w.cities.push_back(cc); }
  w.units.clear();
  if (j.contains("units")) for (const auto& u : j["units"]) { spawn_unit(w, u.value("team",0u), parse_unit(u.value("type", std::string("Infantry"))), {u["pos"][0].get<float>(), u["pos"][1].get<float>()}); }
  if (j.contains("airUnits")) for (const auto& a : j["airUnits"]) { AirUnit au{}; au.id=a.value("id",(uint32_t)(w.airUnits.size()+1)); au.team=a.value("team",0u); au.cls=(AirUnitClass)a.value("class",0); au.state=(AirMissionState)a.value("state",0); au.pos={a["pos"][0].get<float>(),a["pos"][1].get<float>()}; au.missionTarget={a["missionTarget"][0].get<float>(),a["missionTarget"][1].get<float>()}; au.hp=a.value("hp",100.0f); au.speed=a.value("speed",6.0f); au.cooldownTicks=a.value("cooldownTicks",0u); au.missionPerformed=a.value("missionPerformed",false); w.airUnits.push_back(au); }
  w.buildings.clear();
  if (j.contains("buildings")) { uint32_t id=1; for (const auto& b : j["buildings"]) { Building bb{}; bb.id=b.value("id",id++); bb.team=b.value("team",0u); bb.type=parse_building(b.value("type",std::string("House"))); bb.pos={b["pos"][0].get<float>(), b["pos"][1].get<float>()}; bb.size=gBuildDefs[bidx(bb.type)].size; bb.underConstruction=b.value("underConstruction", false); bb.buildProgress=bb.underConstruction?b.value("buildProgress",0.0f):1.0f; bb.buildTime=gBuildDefs[bidx(bb.type)].buildTime; bb.maxHp=(bb.type==BuildingType::CityCenter?2200.0f:1000.0f); bb.hp=b.value("hp", bb.maxHp); w.buildings.push_back(bb);} }
  if (j.contains("strategicStrikes")) for (const auto& st : j["strategicStrikes"]) { StrategicStrike ss{}; ss.id=st.value("id",(uint32_t)(w.strategicStrikes.size()+1)); ss.team=st.value("team",0u); ss.type=(StrikeType)st.value("type",0); ss.from={st["from"][0].get<float>(),st["from"][1].get<float>()}; ss.target={st["target"][0].get<float>(),st["target"][1].get<float>()}; ss.prepTicksRemaining=st.value("prepTicksRemaining",0u); ss.travelTicksRemaining=st.value("travelTicksRemaining",0u); ss.cooldownTicks=st.value("cooldownTicks",0u); ss.interceptionState=st.value("interceptionState",(uint8_t)0); ss.launched=st.value("launched",false); ss.resolved=st.value("resolved",false); w.strategicStrikes.push_back(ss); }
  if (j.contains("denialZones")) for (const auto& dz : j["denialZones"]) { DenialZone z{}; z.id=dz.value("id",(uint32_t)(w.denialZones.size()+1)); z.team=dz.value("team",0u); z.pos={dz["pos"][0].get<float>(),dz["pos"][1].get<float>()}; z.radius=dz.value("radius",6.0f); z.ticksRemaining=dz.value("ticksRemaining",0u); w.denialZones.push_back(z); }
  w.resourceNodes.clear();
  w.roads.clear();
  if (j.contains("roads")) { uint32_t rid=1; for (const auto& rr : j["roads"]) { RoadSegment r{}; r.id = rr.value("id", rid++); r.owner = rr.value("owner", (uint16_t)UINT16_MAX); r.a = {rr["a"][0].get<int>(), rr["a"][1].get<int>()}; r.b = {rr["b"][0].get<int>(), rr["b"][1].get<int>()}; r.quality = rr.value("quality", 1); w.roads.push_back(r);} }
  w.railNodes.clear();
  if (j.contains("railNodes") && j["railNodes"].is_array()) { uint32_t nid = 1; for (const auto& rn : j["railNodes"]) { RailNode n{}; n.id = rn.value("id", nid++); n.owner = rn.value("owner", (uint16_t)UINT16_MAX); n.type = parse_rail_node_type(rn.value("type", std::string("Junction"))); n.tile = {rn["tile"][0].get<int>(), rn["tile"][1].get<int>()}; n.networkId = rn.value("networkId", 0u); n.active = rn.value("active", true); w.railNodes.push_back(n); } }
  w.railEdges.clear();
  if (j.contains("railEdges") && j["railEdges"].is_array()) { uint32_t eid = 1; for (const auto& re : j["railEdges"]) { RailEdge e{}; e.id = re.value("id", eid++); e.owner = re.value("owner", (uint16_t)UINT16_MAX); e.aNode = re.value("aNode", 0u); e.bNode = re.value("bNode", 0u); e.quality = re.value("quality", 1); e.bridge = re.value("bridge", false); e.tunnel = re.value("tunnel", false); e.disrupted = re.value("disrupted", false); w.railEdges.push_back(e); } }
  w.railNetworks.clear();
  if (j.contains("railNetworks") && j["railNetworks"].is_array()) { for (const auto& rr : j["railNetworks"]) { RailNetwork rn{}; rn.id = rr.value("id", 0u); rn.owner = rr.value("owner", (uint16_t)UINT16_MAX); rn.nodeCount = rr.value("nodeCount", 0u); rn.edgeCount = rr.value("edgeCount", 0u); rn.active = rr.value("active", false); w.railNetworks.push_back(rn); } }
  w.trains.clear();
  if (j.contains("trains") && j["trains"].is_array()) { uint32_t tid=1; for (const auto& tj : j["trains"]) { Train t{}; t.id = tj.value("id", tid++); t.owner = tj.value("owner", (uint16_t)UINT16_MAX); t.type = parse_train_type(tj.value("type", std::string("Supply"))); t.state = (TrainState)tj.value("state", 1); t.currentNode = tj.value("currentNode", 0u); t.destinationNode = tj.value("destinationNode", 0u); t.currentEdge = tj.value("currentEdge", 0u); t.routeCursor = tj.value("routeCursor", 0u); t.segmentProgress = tj.value("segmentProgress", 0.0f); t.speed = tj.value("speed", 0.03f); t.cargo = tj.value("cargo", 0.0f); t.capacity = tj.value("capacity", 100.0f); t.cargoType = tj.value("cargoType", std::string("Supply")); t.lastRouteTick = tj.value("lastRouteTick", 0u); if (tj.contains("route") && tj["route"].is_array()) { for (const auto& rj : tj["route"]) t.route.push_back({rj.value("edgeId", 0u), rj.value("toNode", 0u)}); } w.trains.push_back(std::move(t)); } }
  if (j.contains("resourceNodes")) { uint32_t id=1; for (const auto& r : j["resourceNodes"]) { ResourceNode rn{}; rn.id=r.value("id",id++); std::string t=r.value("type",std::string("Forest")); rn.type=(t=="Ore"?ResourceNodeType::Ore:(t=="Farmable"?ResourceNodeType::Farmable:(t=="Ruins"?ResourceNodeType::Ruins:ResourceNodeType::Forest))); rn.pos={r["pos"][0].get<float>(), r["pos"][1].get<float>()}; rn.amount=r.value("amount",1000.0f); rn.owner=r.value("owner",(uint16_t)UINT16_MAX); w.resourceNodes.push_back(rn);} }
  if (j.contains("mythicGuardians") && j["mythicGuardians"].is_object()) {
    const auto& mg = j["mythicGuardians"];
    if (mg.contains("sites") && mg["sites"].is_array()) {
      w.guardianSites.clear();
      uint32_t sid = 1;
      for (const auto& sj : mg["sites"]) {
        GuardianSiteInstance s{};
        s.instanceId = sj.value("instance_id", sid++);
        s.guardianId = sj.value("guardian_id", std::string("snow_yeti"));
        s.siteType = parse_guardian_site_type(sj.value("site_type", std::string("yeti_lair")));
        s.pos = {sj["pos"][0].get<float>(), sj["pos"][1].get<float>()};
        s.regionId = sj.value("region_id", -1);
        s.nodeId = sj.value("node_id", -1);
        s.discovered = sj.value("discovered", false);
        s.alive = sj.value("alive", true);
        s.owner = sj.value("owner", (uint16_t)UINT16_MAX);
        s.siteActive = sj.value("site_active", true);
        s.siteDepleted = sj.value("site_depleted", false);
        s.spawned = sj.value("spawned", false);
        s.behaviorState = sj.value("behavior_state", (uint8_t)0);
        s.cooldownTicks = sj.value("cooldown_ticks", 0u);
        s.oneShotUsed = sj.value("one_shot_used", false);
        s.scenarioPlaced = true;
        w.guardianSites.push_back(std::move(s));
      }
      std::sort(w.guardianSites.begin(), w.guardianSites.end(), [](const GuardianSiteInstance& a, const GuardianSiteInstance& b){ return a.instanceId < b.instanceId; });
    }
    if (mg.contains("counters") && mg["counters"].is_object()) {
      const auto& c = mg["counters"];
      w.guardiansDiscovered = c.value("discovered", w.guardiansDiscovered);
      w.guardiansSpawned = c.value("spawned", w.guardiansSpawned);
      w.guardiansJoined = c.value("joined", w.guardiansJoined);
      w.guardiansKilled = c.value("killed", w.guardiansKilled);
      w.hostileGuardianEvents = c.value("hostile_events", w.hostileGuardianEvents);
      w.alliedGuardianEvents = c.value("allied_events", w.alliedGuardianEvents);
    }
  }
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
  if (j.contains("objectives")) for (const auto& o : j["objectives"]) {
    Objective ob{}; ob.id=o.value("id",0u); ob.objectiveId=o.value("objective_id",std::to_string(ob.id)); ob.title=o.value("title",""); ob.description=o.value("description",o.value("text",std::string(""))); ob.text=ob.description; ob.primary=o.value("primary",true); ob.category=parse_objective_category(o.value("category", ob.primary?std::string("primary"):std::string("secondary"))); ob.state=parse_objective_state(o.value("state",std::string("inactive"))); ob.owner=o.value("owner",(uint16_t)UINT16_MAX); ob.visible=o.value("visible",true); ob.progressText=o.value("progressText",std::string("")); ob.progressValue=o.value("progressValue",0.0f); w.objectives.push_back(ob);
  }
  w.triggers.clear();
  if (j.contains("triggers")) for (const auto& t : j["triggers"]) {
    Trigger tr{}; tr.id=t.value("id",0u); tr.once=t.value("once",true); auto c=t["condition"]; std::string ctype=c.value("type",std::string("TickReached"));
    if (ctype=="UnitDestroyed" || ctype=="EntityDestroyed") tr.condition.type=TriggerType::UnitDestroyed; else if (ctype=="BuildingDestroyed") tr.condition.type=TriggerType::BuildingDestroyed; else if (ctype=="BuildingCompleted") tr.condition.type=TriggerType::BuildingCompleted; else if (ctype=="ObjectiveCompleted") tr.condition.type=TriggerType::ObjectiveCompleted; else if (ctype=="ObjectiveFailed") tr.condition.type=TriggerType::ObjectiveFailed; else if (ctype=="AreaEntered") tr.condition.type=TriggerType::AreaEntered; else if (ctype=="PlayerEliminated") tr.condition.type=TriggerType::PlayerEliminated; else tr.condition.type=TriggerType::TickReached;
    tr.condition.tick=c.value("tick",0u); tr.condition.entityId=c.value("entityId",0u); tr.condition.buildingType=parse_building(c.value("buildingType",std::string("House"))); tr.condition.areaId=c.value("areaId",0u); tr.condition.player=c.value("player",(uint16_t)UINT16_MAX); tr.condition.objectiveId=c.value("objectiveId",0u);
    if (t.contains("actions")) for (const auto& a : t["actions"]) { TriggerAction ac{}; std::string at=a.value("type",std::string("ShowMessage")); if (at=="ActivateObjective") ac.type=TriggerActionType::ActivateObjective; else if (at=="CompleteObjective") ac.type=TriggerActionType::CompleteObjective; else if (at=="FailObjective") ac.type=TriggerActionType::FailObjective; else if (at=="GrantResources") ac.type=TriggerActionType::GrantResources; else if (at=="SpawnUnits") ac.type=TriggerActionType::SpawnUnits; else if (at=="EndMatchVictory") ac.type=TriggerActionType::EndMatchVictory; else if (at=="EndMatchDefeat") ac.type=TriggerActionType::EndMatchDefeat; else if (at=="RunLuaHook") ac.type=TriggerActionType::RunLuaHook; else ac.type=TriggerActionType::ShowMessage; ac.text=a.value("text",""); ac.objectiveId=a.value("objectiveId",0u); ac.player=a.value("player",(uint16_t)UINT16_MAX); if (a.contains("resources")) { auto r=a["resources"]; ac.resources[ridx(Resource::Food)] = r.value("Food",0.0f); ac.resources[ridx(Resource::Wood)] = r.value("Wood",0.0f); ac.resources[ridx(Resource::Metal)] = r.value("Metal",0.0f); ac.resources[ridx(Resource::Wealth)] = r.value("Wealth",0.0f); ac.resources[ridx(Resource::Knowledge)] = r.value("Knowledge",0.0f); ac.resources[ridx(Resource::Oil)] = r.value("Oil",0.0f); } ac.spawnUnitType=parse_unit(a.value("unitType",std::string("Infantry"))); ac.spawnCount=a.value("count",0u); if (a.contains("pos")) ac.spawnPos={a["pos"][0].get<float>(),a["pos"][1].get<float>()}; ac.winner=a.value("winner",0u); ac.areaId=a.value("areaId",0u); ac.luaHook=a.value("luaHook",std::string("")); tr.actions.push_back(ac);} w.triggers.push_back(tr);} 
  if (j.contains("mission")) {
    const auto& m = j["mission"];
    w.mission.title = m.value("title", std::string(""));
    w.mission.briefing = m.value("briefing", std::string(""));
    if (m.contains("introMessages") && m["introMessages"].is_array()) w.mission.introMessages = m["introMessages"].get<std::vector<std::string>>();
    w.mission.victoryOutcomeTag = m.value("victoryOutcome", std::string("victory"));
    w.mission.defeatOutcomeTag = m.value("defeatOutcome", std::string("defeat"));
    w.mission.partialOutcomeTag = m.value("partialOutcome", std::string("partial_victory"));
    w.mission.branchKey = m.value("branchKey", std::string(""));
    w.mission.luaScriptFile = m.value("luaScript", std::string(""));
    w.mission.luaScriptInline = m.value("luaInline", std::string(""));
    for (const auto& msg : w.mission.introMessages) w.objectiveLog.push_back({w.tick, msg});
  }
  rebuild_mountain_regions(w);
  if (j.contains("mountainRegions") && j["mountainRegions"].is_array()) {
    w.mountainRegions.clear();
    for (const auto& mr : j["mountainRegions"]) {
      MountainRegion m{};
      m.id = mr.value("id", (uint32_t)(w.mountainRegions.size()+1));
      m.minX = mr.value("minX", 0); m.minY = mr.value("minY", 0); m.maxX = mr.value("maxX", 0); m.maxY = mr.value("maxY", 0);
      m.peakCell = mr.value("peakCell", -1); m.centerCell = mr.value("centerCell", -1); m.cellCount = mr.value("cellCount", 0u);
      w.mountainRegions.push_back(m);
    }
  }
  w.surfaceDeposits.clear();
  if (j.contains("surfaceDeposits") && j["surfaceDeposits"].is_array()) {
    for (const auto& sdj : j["surfaceDeposits"]) {
      SurfaceDeposit sd{};
      sd.id = sdj.value("id", (uint32_t)(w.surfaceDeposits.size()+1));
      sd.regionId = sdj.value("regionId", 0u);
      sd.mineral = static_cast<MineralType>(sdj.value("mineral", 1));
      sd.cell = sdj.value("cell", -1);
      sd.remaining = sdj.value("remaining", 800.0f);
      sd.owner = sdj.value("owner", (uint16_t)UINT16_MAX);
      w.surfaceDeposits.push_back(sd);
    }
  }
  w.deepDeposits.clear();
  if (j.contains("deepDeposits") && j["deepDeposits"].is_array()) {
    for (const auto& ddj : j["deepDeposits"]) {
      DeepDeposit dd{};
      dd.id = ddj.value("id", (uint32_t)(w.deepDeposits.size()+1));
      dd.regionId = ddj.value("regionId", 0u);
      dd.nodeId = ddj.value("nodeId", 0u);
      dd.mineral = static_cast<MineralType>(ddj.value("mineral", 0));
      dd.cell = ddj.value("cell", -1);
      dd.richness = ddj.value("richness", 1.5f);
      dd.remaining = ddj.value("remaining", 1600.0f);
      dd.owner = ddj.value("owner", (uint16_t)UINT16_MAX);
      dd.active = ddj.value("active", true);
      w.deepDeposits.push_back(dd);
    }
  }
  w.undergroundNodes.clear();
  if (j.contains("undergroundNodes") && j["undergroundNodes"].is_array()) {
    for (const auto& nj : j["undergroundNodes"]) {
      UndergroundNode n{};
      n.id = nj.value("id", (uint32_t)(w.undergroundNodes.size()+1));
      n.regionId = nj.value("regionId", 0u);
      n.type = static_cast<UndergroundNodeType>(nj.value("type", 2));
      n.cell = nj.value("cell", -1);
      n.linkedBuildingId = nj.value("linkedBuildingId", 0u);
      n.owner = nj.value("owner", (uint16_t)UINT16_MAX);
      n.active = nj.value("active", true);
      w.undergroundNodes.push_back(n);
    }
  }
  w.undergroundEdges.clear();
  if (j.contains("undergroundEdges") && j["undergroundEdges"].is_array()) {
    for (const auto& ej : j["undergroundEdges"]) {
      UndergroundEdge e{};
      e.id = ej.value("id", (uint32_t)(w.undergroundEdges.size()+1));
      e.regionId = ej.value("regionId", 0u);
      e.a = ej.value("a", 0u); e.b = ej.value("b", 0u);
      e.owner = ej.value("owner", (uint16_t)UINT16_MAX);
      e.active = ej.value("active", true);
      w.undergroundEdges.push_back(e);
    }
  }
  if (w.deepDeposits.empty() || w.undergroundNodes.empty() || w.undergroundEdges.empty()) rebuild_mountain_deposits(w);

  for (auto& s : w.guardianSites) {
    if (s.spawned) {
      const GuardianDefinition* d = find_guardian_definition(w, s.guardianId);
      if (d) {
        if (s.owner == UINT16_MAX) s.owner = 0;
        spawn_guardian_unit(w, s, *d);
      }
    }
  }

  w.missionRuntime.status = MissionStatus::Running;
  w.missionRuntime.briefingShown = false;
  recompute_population(w);
  recompute_territory(w);
  recompute_fog(w);
  return true;
}

bool save_scenario_file(const std::string& path, const World& w, std::string& err) {
  nlohmann::json j;
  j["schemaVersion"] = 1; j["seed"] = w.seed; j["mapWidth"] = w.width; j["mapHeight"] = w.height;
  j["worldPreset"] = world_preset_name(w.worldPreset);
  j["players"] = nlohmann::json::array();
  for (const auto& p : w.players) j["players"].push_back({{"id",p.id},{"age",(int)p.age},{"resources",p.resources},{"popCap",p.popCap},{"isHuman",p.isHuman},{"isCPU",p.isCPU},{"team",p.teamId},{"civilization",p.civilization.id},{"color",{p.color[0],p.color[1],p.color[2]}},{"startingResources",{{"Food",p.resources[0]},{"Wood",p.resources[1]},{"Metal",p.resources[2]},{"Wealth",p.resources[3]},{"Knowledge",p.resources[4]},{"Oil",p.resources[5]}}}});
  j["cities"] = nlohmann::json::array(); for (const auto& c : w.cities) j["cities"].push_back({{"id",c.id},{"team",c.team},{"pos",{c.pos.x,c.pos.y}},{"level",c.level},{"capital",c.capital}});
  j["units"] = nlohmann::json::array(); for (const auto& u : w.units) j["units"].push_back({{"id",u.id},{"team",u.team},{"type",unit_name(u.type)},{"pos",{u.pos.x,u.pos.y}}});
  j["buildings"] = nlohmann::json::array(); for (const auto& b : w.buildings) j["buildings"].push_back({{"id",b.id},{"team",b.team},{"type",building_name(b.type)},{"pos",{b.pos.x,b.pos.y}},{"underConstruction",b.underConstruction},{"buildProgress",b.buildProgress},{"hp",b.hp}});
  j["airUnits"] = nlohmann::json::array(); for (const auto& a : w.airUnits) j["airUnits"].push_back({{"id",a.id},{"team",a.team},{"class",(int)a.cls},{"state",(int)a.state},{"pos",{a.pos.x,a.pos.y}},{"missionTarget",{a.missionTarget.x,a.missionTarget.y}},{"hp",a.hp},{"speed",a.speed},{"cooldownTicks",a.cooldownTicks},{"missionPerformed",a.missionPerformed}});
  j["strategicStrikes"] = nlohmann::json::array(); for (const auto& st : w.strategicStrikes) j["strategicStrikes"].push_back({{"id",st.id},{"team",st.team},{"type",(int)st.type},{"from",{st.from.x,st.from.y}},{"target",{st.target.x,st.target.y}},{"prepTicksRemaining",st.prepTicksRemaining},{"travelTicksRemaining",st.travelTicksRemaining},{"cooldownTicks",st.cooldownTicks},{"interceptionState",st.interceptionState},{"launched",st.launched},{"resolved",st.resolved}});
  j["denialZones"] = nlohmann::json::array(); for (const auto& dz : w.denialZones) j["denialZones"].push_back({{"id",dz.id},{"team",dz.team},{"pos",{dz.pos.x,dz.pos.y}},{"radius",dz.radius},{"ticksRemaining",dz.ticksRemaining}});
  j["resourceNodes"] = nlohmann::json::array(); for (const auto& r : w.resourceNodes) { std::string t="Forest"; if (r.type==ResourceNodeType::Ore) t="Ore"; else if (r.type==ResourceNodeType::Farmable) t="Farmable"; else if (r.type==ResourceNodeType::Ruins) t="Ruins"; j["resourceNodes"].push_back({{"id",r.id},{"type",t},{"pos",{r.pos.x,r.pos.y}},{"amount",r.amount},{"owner",r.owner}}); }
  j["mountainRegions"] = nlohmann::json::array(); for (const auto& mr : w.mountainRegions) j["mountainRegions"].push_back({{"id",mr.id},{"minX",mr.minX},{"minY",mr.minY},{"maxX",mr.maxX},{"maxY",mr.maxY},{"peakCell",mr.peakCell},{"centerCell",mr.centerCell},{"cellCount",mr.cellCount}});
  j["surfaceDeposits"] = nlohmann::json::array(); for (const auto& sd : w.surfaceDeposits) j["surfaceDeposits"].push_back({{"id",sd.id},{"regionId",sd.regionId},{"mineral",(int)sd.mineral},{"cell",sd.cell},{"remaining",sd.remaining},{"owner",sd.owner}});
  j["deepDeposits"] = nlohmann::json::array(); for (const auto& dd : w.deepDeposits) j["deepDeposits"].push_back({{"id",dd.id},{"regionId",dd.regionId},{"nodeId",dd.nodeId},{"mineral",(int)dd.mineral},{"cell",dd.cell},{"richness",dd.richness},{"remaining",dd.remaining},{"owner",dd.owner},{"active",dd.active}});
  j["undergroundNodes"] = nlohmann::json::array(); for (const auto& n : w.undergroundNodes) j["undergroundNodes"].push_back({{"id",n.id},{"regionId",n.regionId},{"type",(int)n.type},{"cell",n.cell},{"linkedBuildingId",n.linkedBuildingId},{"owner",n.owner},{"active",n.active}});
  j["undergroundEdges"] = nlohmann::json::array(); for (const auto& e : w.undergroundEdges) j["undergroundEdges"].push_back({{"id",e.id},{"regionId",e.regionId},{"a",e.a},{"b",e.b},{"owner",e.owner},{"active",e.active}});
  j["roads"] = nlohmann::json::array(); for (const auto& r : w.roads) j["roads"].push_back({{"id",r.id},{"owner",r.owner},{"a",{r.a.x,r.a.y}},{"b",{r.b.x,r.b.y}},{"quality",r.quality}});
  j["railNodes"] = nlohmann::json::array(); for (const auto& n : w.railNodes) j["railNodes"].push_back({{"id",n.id},{"owner",n.owner},{"type",rail_node_type_name(n.type)},{"tile",{n.tile.x,n.tile.y}},{"networkId",n.networkId},{"active",n.active}});
  j["railEdges"] = nlohmann::json::array(); for (const auto& e : w.railEdges) j["railEdges"].push_back({{"id",e.id},{"owner",e.owner},{"aNode",e.aNode},{"bNode",e.bNode},{"quality",e.quality},{"bridge",e.bridge},{"tunnel",e.tunnel},{"disrupted",e.disrupted}});
  j["railNetworks"] = nlohmann::json::array(); for (const auto& rn : w.railNetworks) j["railNetworks"].push_back({{"id",rn.id},{"owner",rn.owner},{"nodeCount",rn.nodeCount},{"edgeCount",rn.edgeCount},{"active",rn.active}});
  j["trains"] = nlohmann::json::array();
  for (const auto& t : w.trains) { nlohmann::json route = nlohmann::json::array(); for (const auto& step : t.route) route.push_back({{"edgeId",step.edgeId},{"toNode",step.toNode}}); j["trains"].push_back({{"id",t.id},{"owner",t.owner},{"type",train_type_name(t.type)},{"state",(int)t.state},{"currentNode",t.currentNode},{"destinationNode",t.destinationNode},{"currentEdge",t.currentEdge},{"routeCursor",t.routeCursor},{"segmentProgress",t.segmentProgress},{"speed",t.speed},{"cargo",t.cargo},{"capacity",t.capacity},{"cargoType",t.cargoType},{"lastRouteTick",t.lastRouteTick},{"route",route}}); }
  j["guardianDefinitions"] = nlohmann::json::array();
  for (const auto& d : w.guardianDefinitions) j["guardianDefinitions"].push_back({{"guardian_id",d.guardianId},{"display_name",d.displayName},{"biome_requirement",(int)d.biomeRequirement},{"site_type",guardian_site_type_name(d.siteType)},{"spawn_mode",guardian_spawn_mode_name(d.spawnMode)},{"max_per_map",d.maxPerMap},{"unique",d.unique},{"discovery_mode",guardian_discovery_mode_name(d.discoveryMode)},{"behavior_mode",guardian_behavior_mode_name(d.behaviorMode)},{"join_mode",guardian_join_mode_name(d.joinMode)},{"associated_unit_definition",d.associatedUnitDefinitionId},{"reward_hook",d.rewardHook},{"effect_hook",d.effectHook},{"scenario_only",d.scenarioOnly},{"procedural",d.procedural},{"rarity_permille",d.rarityPermille},{"min_spacing_cells",d.minSpacingCells},{"discovery_radius",d.discoveryRadius},{"unit",{{"hp",d.unitHp},{"attack",d.unitAttack},{"range",d.unitRange},{"speed",d.unitSpeed}}}});
  j["mythicGuardians"] = {{"sites", nlohmann::json::array()}, {"counters", {{"discovered",w.guardiansDiscovered},{"spawned",w.guardiansSpawned},{"joined",w.guardiansJoined},{"killed",w.guardiansKilled},{"hostile_events",w.hostileGuardianEvents},{"allied_events",w.alliedGuardianEvents}}}};
  for (const auto& s : w.guardianSites) {
    j["mythicGuardians"]["sites"].push_back({
      {"instance_id", s.instanceId}, {"guardian_id", s.guardianId}, {"site_type", guardian_site_type_name(s.siteType)}, {"pos", {s.pos.x, s.pos.y}},
      {"region_id", s.regionId}, {"node_id", s.nodeId}, {"discovered", s.discovered}, {"alive", s.alive}, {"owner", s.owner},
      {"site_active", s.siteActive}, {"site_depleted", s.siteDepleted}, {"spawned", s.spawned}, {"behavior_state", s.behaviorState},
      {"cooldown_ticks", s.cooldownTicks}, {"one_shot_used", s.oneShotUsed}, {"scenario_placed", s.scenarioPlaced}
    });
  }
  j["biomeMap"] = w.biomeMap;
  j["temperatureMap"] = w.temperatureMap;
  j["moistureMap"] = w.moistureMap;
  j["coastClassMap"] = w.coastClassMap;
  j["landmassIdByCell"] = w.landmassIdByCell;
  j["riverMap"] = w.riverMap;
  j["lakeMap"] = w.lakeMap;
  j["resourceWeightMap"] = w.resourceWeightMap;
  j["startCandidates"] = nlohmann::json::array();
  for (const auto& sc : w.startCandidates) j["startCandidates"].push_back({{"cell",sc.cell},{"score",sc.score},{"civBiasMask",sc.civBiasMask}});
  j["mythicCandidates"] = nlohmann::json::array();
  for (const auto& mc : w.mythicCandidates) j["mythicCandidates"].push_back({{"siteType",(int)mc.siteType},{"cell",mc.cell},{"score",mc.score}});
  j["areas"] = nlohmann::json::array(); for (const auto& a : w.triggerAreas) j["areas"].push_back({{"id",a.id},{"min",{a.min.x,a.min.y}},{"max",{a.max.x,a.max.y}}});
  j["objectives"] = nlohmann::json::array(); for (const auto& o : w.objectives) j["objectives"].push_back({{"id",o.id},{"objective_id",o.objectiveId},{"title",o.title},{"description",o.description.empty()?o.text:o.description},{"primary",o.primary},{"category",objective_category_name(o.category)},{"state",objective_state_name(o.state)},{"owner",o.owner},{"visible",o.visible},{"progressText",o.progressText},{"progressValue",o.progressValue}});
  j["mission"] = {{"title",w.mission.title},{"briefing",w.mission.briefing},{"introMessages",w.mission.introMessages},{"victoryOutcome",w.mission.victoryOutcomeTag},{"defeatOutcome",w.mission.defeatOutcomeTag},{"partialOutcome",w.mission.partialOutcomeTag},{"branchKey",w.mission.branchKey},{"luaScript",w.mission.luaScriptFile},{"luaInline",w.mission.luaScriptInline}};
  j["triggers"] = nlohmann::json::array();
  for (const auto& t : w.triggers) {
    nlohmann::json jt; jt["id"]=t.id; jt["once"]=t.once;
    std::string ctype="TickReached"; if (t.condition.type==TriggerType::UnitDestroyed) ctype="UnitDestroyed"; else if (t.condition.type==TriggerType::BuildingDestroyed) ctype="BuildingDestroyed"; else if (t.condition.type==TriggerType::BuildingCompleted) ctype="BuildingCompleted"; else if (t.condition.type==TriggerType::ObjectiveCompleted) ctype="ObjectiveCompleted"; else if (t.condition.type==TriggerType::ObjectiveFailed) ctype="ObjectiveFailed"; else if (t.condition.type==TriggerType::AreaEntered) ctype="AreaEntered"; else if (t.condition.type==TriggerType::PlayerEliminated) ctype="PlayerEliminated"; else if (t.condition.type==TriggerType::DiplomacyChanged) ctype="DiplomacyChanged"; else if (t.condition.type==TriggerType::WorldTensionReached) ctype="WorldTensionReached"; else if (t.condition.type==TriggerType::StrategicStrikeLaunched) ctype="StrategicStrikeLaunched"; else if (t.condition.type==TriggerType::WonderCompleted) ctype="WonderCompleted"; else if (t.condition.type==TriggerType::CargoLanded) ctype="CargoLanded";
    std::string rel="Neutral"; if (t.condition.diplomacy==DiplomacyRelation::Allied) rel="Allied"; else if (t.condition.diplomacy==DiplomacyRelation::War) rel="War"; else if (t.condition.diplomacy==DiplomacyRelation::Ceasefire) rel="Ceasefire";
    jt["condition"]={{"type",ctype},{"tick",t.condition.tick},{"entityId",t.condition.entityId},{"buildingType",building_name(t.condition.buildingType)},{"areaId",t.condition.areaId},{"player",t.condition.player},{"objectiveId",t.condition.objectiveId},{"worldTension",t.condition.worldTension},{"playerB",t.condition.playerB},{"relation",rel}};
    jt["actions"]=nlohmann::json::array();
    for (const auto& a : t.actions) {
      std::string at="ShowMessage"; if (a.type==TriggerActionType::ActivateObjective) at="ActivateObjective"; else if (a.type==TriggerActionType::CompleteObjective) at="CompleteObjective"; else if (a.type==TriggerActionType::FailObjective) at="FailObjective"; else if (a.type==TriggerActionType::GrantResources) at="GrantResources"; else if (a.type==TriggerActionType::SpawnUnits) at="SpawnUnits"; else if (a.type==TriggerActionType::SpawnBuildings) at="SpawnBuildings"; else if (a.type==TriggerActionType::ChangeDiplomacy) at="ChangeDiplomacy"; else if (a.type==TriggerActionType::SetWorldTension) at="SetWorldTension"; else if (a.type==TriggerActionType::RevealArea) at="RevealArea"; else if (a.type==TriggerActionType::LaunchOperation) at="LaunchOperation"; else if (a.type==TriggerActionType::EndMatchVictory) at="EndMatchVictory"; else if (a.type==TriggerActionType::EndMatchDefeat) at="EndMatchDefeat"; else if (a.type==TriggerActionType::RunLuaHook) at="RunLuaHook";
      std::string rel="Neutral"; if (a.diplomacy==DiplomacyRelation::Allied) rel="Allied"; else if (a.diplomacy==DiplomacyRelation::War) rel="War"; else if (a.diplomacy==DiplomacyRelation::Ceasefire) rel="Ceasefire";
      jt["actions"].push_back({{"type",at},{"text",a.text},{"objectiveId",a.objectiveId},{"state",objective_state_name(a.objectiveState)},{"player",a.player},{"playerB",a.playerB},{"resources",{{"Food",a.resources[0]},{"Wood",a.resources[1]},{"Metal",a.resources[2]},{"Wealth",a.resources[3]},{"Knowledge",a.resources[4]},{"Oil",a.resources[5]}}},{"unitType",unit_name(a.spawnUnitType)},{"buildingType",building_name(a.spawnBuildingType)},{"count",a.spawnCount},{"pos",{a.spawnPos.x,a.spawnPos.y}},{"winner",a.winner},{"areaId",a.areaId},{"relation",rel},{"worldTension",a.worldTension},{"operationType",(int)a.operationType},{"operationTarget",{a.operationTarget.x,a.operationTarget.y}},{"luaHook",a.luaHook}});
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
  gPendingNavRequests.clear();
  gCompletedNavResults.clear();
  gNextNavRequestId = 1;
  gNav.nextMoveOrder = 1;
  gSpatial.cells.clear();
  ++w.navVersion;
  w.territoryDirty = true;
  w.fogDirty = true;
  w.uiBuildMenu = false;
  w.uiTrainMenu = false;
  w.uiResearchMenu = false;
  w.placementActive = false;
  std::sort(w.guardianDefinitions.begin(), w.guardianDefinitions.end(), [](const GuardianDefinition& a, const GuardianDefinition& b){ return a.guardianId < b.guardianId; });
  std::sort(w.guardianSites.begin(), w.guardianSites.end(), [](const GuardianSiteInstance& a, const GuardianSiteInstance& b){ return a.instanceId < b.instanceId; });
  if (w.mountainRegionByCell.empty()) rebuild_mountain_regions(w);
  if (w.deepDeposits.empty() || w.undergroundNodes.empty() || w.undergroundEdges.empty()) rebuild_mountain_deposits(w);
  update_underground_economy(w, 0.0f);
  w.gameOver = w.match.phase != MatchPhase::Running;
  w.winner = w.match.winner;
}

void tick_world(World& w, float dt) {
  using Clock = std::chrono::steady_clock;
  const auto tickStart = Clock::now();
  gLastStats = {};
  gLastStats.threads = static_cast<uint32_t>(worker_threads());
  ++w.tick;
  if (w.match.phase != MatchPhase::Running) {
    if (w.match.phase == MatchPhase::Ended) w.match.phase = MatchPhase::Postmatch;
    return;
  }
  if (w.tick % 10 == 0) recompute_territory(w);

  process_nav_requests(w);
  apply_nav_results(w);

  spatial_prepare(w);
  ensure_base_roads(w);
  ensure_base_rail(w);
  recompute_rail_networks(w);
  ensure_trains(w);
  update_trains(w);
  apply_rail_logistics(w);
  recompute_trade_routes(w);
  recompute_supply(w);
  update_espionage(w);
  update_world_tension(w);
  update_ai_diplomacy(w);
  update_operations(w);
  rebuild_detector_sites(w);
  update_air_and_strategic_warfare(w, dt);
  update_guardian_sites(w);

  for (auto& p : w.players) p.resources[ridx(Resource::Food)] += 0.4f * dt * 20.0f;
  apply_trade_income(w, dt);
  update_underground_economy(w, dt);

  for (auto& b : w.buildings) {
    auto& owner = w.players[b.team];
    const BuildDef& def = gBuildDefs[bidx(b.type)];
    if (b.underConstruction) {
      float workerFactor = has_nearby_builder(w, b.team, b.pos) ? 1.0f : 0.25f;
      const float civBuild = owner.civilization.buildingBuildTimeMult[static_cast<size_t>(b.type)];
      b.buildProgress += dt / std::max(1.0f, b.buildTime * civBuild) * workerFactor;
      if (b.buildProgress >= 1.0f) { b.underConstruction = false; ++w.completedBuildingsCount; emit_event(w, GameplayEventType::BuildingCompleted, b.team, b.team, b.id); if (b.type == BuildingType::Wonder) emit_event(w, GameplayEventType::WonderStarted, b.team, b.team, b.id); ++w.navVersion; gNav.cache.clear(); }
      continue;
    }

    float op = has_nearby_builder(w, b.team, b.pos) ? 1.0f : 0.0f;
    for (size_t r = 0; r < static_cast<size_t>(Resource::Count); ++r) {
      float trick = def.trickle[r];
      if (trick > 0.0f) {
        float mult = owner.civilization.buildingTrickleMult[static_cast<size_t>(b.type)];
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

  for (auto& tr : w.units) {
    if (tr.hp <= 0 || tr.type != UnitType::TransportShip || tr.embarked) continue;
    if (tr.cargo.size() < 4) {
      for (auto& lu : w.units) {
        if (lu.team != tr.team || lu.embarked || !unit_can_embark(lu.type)) continue;
        if (dist(lu.pos, tr.pos) < 8.0f) {
          lu.embarked = true;
          lu.transportId = tr.id;
          lu.hasMoveOrder = false;
          lu.pos = tr.pos;
          tr.cargo.push_back(lu.id);
          ++w.embarkEvents;
          break;
        }
      }
    }
    if (!tr.cargo.empty() && (w.tick % 60 == 0)) {
      uint32_t uid = tr.cargo.back();
      tr.cargo.pop_back();
      for (auto& lu : w.units) if (lu.id == uid) {
        lu.embarked = false;
        lu.transportId = 0;
        lu.pos = tr.pos + glm::vec2{0.8f, 0.4f};
        ++w.disembarkEvents;
        break;
      }
    }
  }

  const auto combatStart = Clock::now();
  rebuild_chunk_membership_impl(w);
  gLastStats.chunkCount = static_cast<uint32_t>(gChunks.chunks.size());

  std::vector<Unit> unitSnapshot = w.units;
  std::unordered_map<uint32_t, size_t> unitIndexById;
  unitIndexById.reserve(unitSnapshot.size());
  for (size_t i = 0; i < unitSnapshot.size(); ++i) unitIndexById[unitSnapshot[i].id] = i;

  std::vector<MovementResult> movementResults(w.units.size());
  TaskGraph movementGraph;
  for (size_t chunkIdx = 0; chunkIdx < gChunks.chunks.size(); ++chunkIdx) {
    movementGraph.jobs.push_back({[&w, &unitSnapshot, &unitIndexById, &movementResults, chunkIdx, dt]() {
      const auto& chunk = gChunks.chunks[chunkIdx];
      for (uint32_t uid : chunk.unitIds) {
        auto it = unitIndexById.find(uid);
        if (it == unitIndexById.end()) continue;
        const Unit& src = unitSnapshot[it->second];
        if (src.hp <= 0 || src.embarked) continue;
        MovementResult out{};
        out.valid = true;
        out.id = src.id;
        glm::vec2 desired = src.target - src.pos;
        if (src.hasMoveOrder && src.moveOrder != 0) {
          desired = src.slotTarget - src.pos;
        }
        glm::vec2 repulse{0.0f, 0.0f};
        for (const auto& other : unitSnapshot) {
          if (other.id == src.id || other.team != src.team || other.hp <= 0) continue;
          glm::vec2 delta = src.pos - other.pos;
          float l = glm::length(delta);
          if (l > 0.001f && l < 1.2f) repulse += (delta / l) * (1.2f - l);
        }
        desired += repulse * 0.65f;
        out.pos = src.pos;
        out.moveDir = src.moveDir;
        out.stuckTicks = src.stuckTicks;
        float len = glm::length(desired);
        if (len > 0.05f) {
          glm::vec2 dir = desired / len;
          out.moveDir = glm::mix(src.moveDir, dir, 0.35f);
          float ml = glm::length(out.moveDir);
          if (ml > 0.001f) out.moveDir /= ml;
          glm::vec2 prev = out.pos;
          float supplyMul = 1.0f;
          if (src.supplyState == SupplyState::LowSupply) supplyMul = 0.9f;
          else if (src.supplyState == SupplyState::OutOfSupply) supplyMul = 0.75f;
          float roadMul = near_friendly_road(w, src.team, src.pos) ? 1.2f : 1.0f;
          out.pos += out.moveDir * src.speed * supplyMul * roadMul * dt;
          int nc = cell_of(w, out.pos);
          if (!unit_cell_valid(w, src, nc)) out.pos = prev;
          if (glm::length(out.pos - prev) < 0.005f && src.hasMoveOrder) {
            out.stuckTicks = (uint16_t)std::min<int>(src.stuckTicks + 1, 65535);
          } else out.stuckTicks = 0;
        }
        out.reachedSlot = src.hasMoveOrder && dist(out.pos, src.slotTarget) < 1.1f;
        movementResults[it->second] = out;
      }
    }});
  }
  gLastStats.movementTasks += static_cast<uint32_t>(movementGraph.jobs.size());
  gLastStats.jobCount += static_cast<uint32_t>(movementGraph.jobs.size());
  run_task_graph(movementGraph);

  std::vector<std::pair<uint32_t, size_t>> commitOrder;
  commitOrder.reserve(w.units.size());
  for (size_t i = 0; i < w.units.size(); ++i) if (movementResults[i].valid) commitOrder.push_back({movementResults[i].id, i});
  std::sort(commitOrder.begin(), commitOrder.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
  for (const auto& entry : commitOrder) {
    auto& u = w.units[entry.second];
    const auto& r = movementResults[entry.second];
    u.pos = r.pos;
    u.moveDir = r.moveDir;
    u.stuckTicks = r.stuckTicks;
    if (u.stuckTicks > 80) ++w.stuckMoveAssertions;
    if (r.reachedSlot) {
      ++w.unitsReachedSlotCount;
      u.hasMoveOrder = false;
      u.moveOrder = 0;
      u.target = u.pos;
      u.attackMove = false;
      u.attackMoveOrder = 0;
    }
  }

  spatial_prepare(w);

  for (auto& u : w.units) {
    if (u.hp <= 0) continue;
    if (u.attackCooldownTicks > 0) --u.attackCooldownTicks;
    if (u.stealthRevealTicks > 0) --u.stealthRevealTicks;
    bool engagedThisTick = false;

    Unit* locked = u.targetUnit ? find_unit(w, u.targetUnit) : nullptr;
    const float aggro = u.attackMove ? (kAttackMoveAggroPermille / 1000.0f) : 7.0f;

    uint32_t candidate = find_enemy_near(w, u, aggro);
    if (!locked && candidate != 0) {
      u.targetUnit = candidate;
      u.targetLockTicks = 0;
      ++w.targetSwitchCount;
      locked = find_unit(w, candidate);
    }

    int buildingTarget = -1;
    if ((u.role == UnitRole::Siege || u.type == UnitType::BombardShip) && (!locked || u.attackMove)) {
      buildingTarget = find_building_target(w, u, aggro + 4.0f);
      if (buildingTarget >= 0 && (!locked || dist(u.pos, w.buildings[buildingTarget].pos) <= u.range + 1.5f)) engagedThisTick = true;
    }

    if (u.type != UnitType::Worker && !u.embarked && u.attackCooldownTicks == 0) {
      if (locked && dist(u.pos, locked->pos) <= u.range + 0.2f) {
        int mult = (int)u.vsRoleMultiplierPermille[role_idx(locked->role)];
        float damage = u.attack * (mult / 1000.0f);
        locked->hp -= damage;
        w.totalDamageDealtPermille += (uint32_t)(damage * 1000.0f);
        ++w.combatEngagementCount;
        if (unit_is_naval(u.type)) ++w.navalCombatEvents;
        u.attackCooldownTicks = (uint16_t)gUnitDefs[uidx(u.type)].attackCooldownTicks;
        u.stealthRevealTicks = 90;
        engagedThisTick = true;
      } else if (buildingTarget >= 0 && dist(u.pos, w.buildings[buildingTarget].pos) <= u.range + 0.8f) {
        float mult = (u.role == UnitRole::Siege) ? 1.8f : 0.8f;
        float damage = u.attack * mult;
        w.buildings[buildingTarget].hp -= damage;
        w.totalDamageDealtPermille += (uint32_t)(damage * 1000.0f);
        ++w.buildingDamageEvents;
        ++w.combatEngagementCount;
        if (unit_is_naval(u.type)) ++w.navalCombatEvents;
        u.attackCooldownTicks = (uint16_t)gUnitDefs[uidx(u.type)].attackCooldownTicks;
        engagedThisTick = true;
      }
    }
    if (engagedThisTick) ++w.combatTicks;
    u.renderPos = glm::mix(u.renderPos, u.pos, 0.35f);
  }
  const auto combatEnd = Clock::now();
  apply_supply_effects(w, dt);
  const size_t beforeUnits = w.units.size();
  w.units.erase(std::remove_if(w.units.begin(), w.units.end(), [&](const Unit& u) {
    if (u.hp > 0) return false;
    if (u.type == UnitType::TransportShip) {
      for (uint32_t cid : u.cargo) for (auto& cu : w.units) if (cu.id == cid) cu.hp = 0;
    }
    w.players[u.team].unitsLost += 1;
    for (auto& s : w.guardianSites) {
      if (u.definitionId != s.guardianId) continue;
      if (dist(u.pos, s.pos) > 8.0f) continue;
      if (s.alive) {
        s.alive = false;
        s.siteDepleted = true;
        s.oneShotUsed = true;
        ++w.guardiansKilled;
        emit_event(w, GameplayEventType::GuardianKilled, s.owner, u.team, s.instanceId, "Mythic guardian slain");
      }
      break;
    }
    emit_event(w, GameplayEventType::UnitDied, u.team, u.team, u.id);
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
    const bool wasAlive = p.alive;
    p.alive = controlled_capitals(w, p.id) > 0;
    if (wasAlive && !p.alive) { emit_event(w, GameplayEventType::PlayerEliminated, p.id, p.id, 0); w.worldTension = std::min(100.0f, w.worldTension + 6.0f); }
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
    if (w.config.allowWonder && w.wonder.heldTicks >= w.config.wonderHoldTicks) { emit_event(w, GameplayEventType::WonderCompleted, wonderOwner, wonderOwner, 0); apply_match_end(w, VictoryCondition::Wonder, wonderOwner, false); }
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

  const auto tickEnd = Clock::now();
  gLastTickProfile.combatMs = std::chrono::duration<double, std::milli>(combatEnd - combatStart).count();
  gLastTickProfile.navMs = std::chrono::duration<double, std::milli>(tickEnd - tickStart).count() - gLastTickProfile.combatMs;

  w.logisticsRoadCount = static_cast<uint32_t>(w.roads.size());
  w.railNodeCount = static_cast<uint32_t>(w.railNodes.size());
  w.railEdgeCount = static_cast<uint32_t>(w.railEdges.size());
  w.activeRailNetworks = 0; for (const auto& rn : w.railNetworks) if (rn.active) ++w.activeRailNetworks;
  w.activeTrains = 0; w.activeSupplyTrains = 0; w.activeFreightTrains = 0;
  for (const auto& t : w.trains) if (t.state == TrainState::Active) { ++w.activeTrains; if (t.type == TrainType::Supply) ++w.activeSupplyTrains; if (t.type == TrainType::Freight) ++w.activeFreightTrains; }
  gLastStats.roadCount = w.logisticsRoadCount;
  gLastStats.activeTradeRoutes = w.logisticsTradeActiveCount;
  gLastStats.railNodeCount = w.railNodeCount;
  gLastStats.railEdgeCount = w.railEdgeCount;
  gLastStats.activeRailNetworks = w.activeRailNetworks;
  gLastStats.activeTrains = w.activeTrains;
  gLastStats.activeSupplyTrains = w.activeSupplyTrains;
  gLastStats.activeFreightTrains = w.activeFreightTrains;
  gLastStats.railThroughput = w.railThroughput;
  gLastStats.disruptedRailRoutes = w.disruptedRailRoutes;
  gLastStats.suppliedUnits = w.suppliedUnits;
  gLastStats.lowSupplyUnits = w.lowSupplyUnits;
  gLastStats.outOfSupplyUnits = w.outOfSupplyUnits;
  gLastStats.operationCount = static_cast<uint32_t>(w.operations.size());
  gLastStats.worldTension = w.worldTension;
  gLastStats.allianceCount = 0;
  gLastStats.warCount = 0;
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t j = i + 1; j < w.players.size(); ++j) {
      const auto r = relation_of(w, static_cast<uint16_t>(i), static_cast<uint16_t>(j));
      if (r == DiplomacyRelation::Allied) ++gLastStats.allianceCount;
      if (r == DiplomacyRelation::War) ++gLastStats.warCount;
    }
  }
  gLastStats.activeEspionageOps = static_cast<uint32_t>(std::count_if(w.espionageOps.begin(), w.espionageOps.end(), [](const EspionageOp& op){ return op.state == EspionageOpState::Active; }));
  gLastStats.postureChanges = w.postureChangeCount;
  gLastStats.diplomacyEvents = w.diplomacyEventCount;
  gLastStats.navalUnitCount = 0;
  gLastStats.transportCount = 0;
  gLastStats.embarkedUnitCount = 0;
  gLastStats.activeNavalOperations = 0;
  gLastStats.coastalTargets = 0;
  gLastStats.navalCombatEvents = w.navalCombatEvents;
  gLastStats.airUnitCount = static_cast<uint32_t>(w.airUnits.size());
  gLastStats.detectorCount = static_cast<uint32_t>(w.detectors.size());
  gLastStats.radarReveals = w.radarRevealEvents;
  gLastStats.strategicStrikes = w.strategicStrikeEvents;
  gLastStats.interceptions = w.interceptionEvents;
  gLastStats.activeDenialZones = static_cast<uint32_t>(w.denialZones.size());
  gLastStats.mountainRegionCount = w.mountainRegionCount;
  gLastStats.mountainChainCount = w.mountainChainCount;
  gLastStats.riverCount = w.riverCount;
  gLastStats.lakeCount = w.lakeCount;
  gLastStats.startCandidateCount = w.startCandidateCount;
  gLastStats.mythicCandidateCount = w.mythicCandidateCount;
  gLastStats.surfaceDepositCount = w.surfaceDepositCount;
  gLastStats.deepDepositCount = w.deepDepositCount;
  gLastStats.activeMineShafts = w.activeMineShafts;
  gLastStats.activeTunnels = w.activeTunnels;
  gLastStats.undergroundDepots = w.undergroundDepots;
  gLastStats.undergroundYield = w.undergroundYield;
  gLastStats.guardianSiteCount = static_cast<uint32_t>(w.guardianSites.size());
  gLastStats.guardiansDiscovered = w.guardiansDiscovered;
  gLastStats.guardiansSpawned = w.guardiansSpawned;
  gLastStats.guardiansJoined = w.guardiansJoined;
  gLastStats.guardiansKilled = w.guardiansKilled;
  gLastStats.hostileGuardianEvents = w.hostileGuardianEvents;
  gLastStats.alliedGuardianEvents = w.alliedGuardianEvents;
  for (const auto& u : w.units) {
    if (unit_is_naval(u.type) && u.hp > 0 && !u.embarked) ++gLastStats.navalUnitCount;
    if (u.type == UnitType::TransportShip && u.hp > 0) ++gLastStats.transportCount;
    if (u.embarked) ++gLastStats.embarkedUnitCount;
  }
  for (const auto& o : w.operations) if (o.active && (o.type == OperationType::AmphibiousAssault || o.type == OperationType::NavalPatrol || o.type == OperationType::CoastalBombard)) ++gLastStats.activeNavalOperations;
  for (const auto& b : w.buildings) {
    int c = cell_of(w, b.pos);
    if (terrain_class_at(w, c) == TerrainClass::Land && has_adjacent_coast(w, b.pos)) ++gLastStats.coastalTargets;
  }

  if (w.match.phase == MatchPhase::Ended) w.match.phase = MatchPhase::Postmatch;
}

TickProfile last_tick_profile() { return gLastTickProfile; }

SimulationStats last_simulation_stats() {
  gLastStats.eventCount = static_cast<uint32_t>(gGameplayEvents.size());
  return gLastStats;
}

void rebuild_chunk_membership(const World& world) { rebuild_chunk_membership_impl(world); }

ChunkRange query_chunk_range(int beginChunk, int maxChunkCount) {
  const int chunkCount = static_cast<int>(gChunks.chunks.size());
  ChunkRange range{};
  range.start = std::clamp(beginChunk, 0, chunkCount);
  range.end = std::clamp(range.start + std::max(0, maxChunkCount), range.start, chunkCount);
  return range;
}

void process_chunk_range(const World& world, ChunkRange range, const std::function<void(int chunkIndex)>& fn) {
  rebuild_chunk_membership_impl(world);
  const int start = std::clamp(range.start, 0, static_cast<int>(gChunks.chunks.size()));
  const int end = std::clamp(range.end, start, static_cast<int>(gChunks.chunks.size()));
  for (int i = start; i < end; ++i) fn(i);
}

bool valid_mine_shaft_placement(const World& world, glm::ivec2 tile) { return valid_mine_shaft_placement_impl(world, tile); }
bool deep_deposit_available(const World& world, uint32_t depositId, uint16_t team) { return deep_deposit_available_impl(world, depositId, team); }

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
  auto cost = gBuildDefs[bidx(world.placementType)].cost;
  const auto& civ = world.players[team].civilization;
  for (size_t i=0;i<cost.size();++i) cost[i] *= civ.buildingCostMult[static_cast<size_t>(world.placementType)];
  if (!spend(world.players[team].resources, cost)) return false;
  uint32_t id = 1; for (const auto& b : world.buildings) id = std::max(id, b.id + 1);
  const auto& d = gBuildDefs[bidx(world.placementType)];
  Building nb{}; nb.id=id; nb.team=team; nb.type=world.placementType; nb.pos=world.placementPos; nb.size=d.size; nb.underConstruction=true; nb.buildProgress=0.0f; nb.buildTime=d.buildTime; nb.maxHp=1000.0f * civ.buildingHpMult[static_cast<size_t>(world.placementType)]; nb.hp=nb.maxHp; nb.definitionId = resolved_building_definition_id(world, team, world.placementType); world.buildings.push_back(std::move(nb));
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
  if (it->type == BuildingType::Barracks && (type == UnitType::Worker || unit_is_naval(type) || unit_is_air(type))) return false;
  if (it->type == BuildingType::Port && (!unit_is_naval(type) || unit_is_air(type))) return false;
  if (it->type == BuildingType::Airbase && !(type == UnitType::Fighter || type == UnitType::Interceptor || type == UnitType::Bomber || type == UnitType::StrategicBomber || type == UnitType::ReconDrone || type == UnitType::StrikeDrone)) return false;
  if (it->type == BuildingType::MissileSilo && !(type == UnitType::TacticalMissile || type == UnitType::StrategicMissile)) return false;
  if (it->type != BuildingType::Port && it->type != BuildingType::Airbase && it->type != BuildingType::MissileSilo && (unit_is_naval(type) || unit_is_air(type))) return false;
  auto& p = world.players[team];
  if (p.popUsed + (int)it->queue.size() + gUnitDefs[uidx(type)].popCost > p.popCap) return false;
  auto cost = gUnitDefs[uidx(type)].cost;
  for (size_t i=0;i<cost.size();++i) cost[i] *= p.civilization.unitCostMult[static_cast<size_t>(type)];
  if (!spend(p.resources, cost)) return false;
  it->queue.push_back({QueueKind::TrainUnit, type, gUnitDefs[uidx(type)].trainTime * p.civilization.unitTrainTimeMult[static_cast<size_t>(type)], 0});
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

const BiomeRuntime& biome_runtime(BiomeType biome) {
  load_biomes_once();
  static BiomeRuntime fallback{"temperate_grassland", "Temperate Grassland", {0.32f, 0.62f, 0.28f}};
  const size_t i = static_cast<size_t>(biome);
  if (i >= gBiomes.size() || gBiomes[i].id.empty()) return fallback;
  static BiomeRuntime out;
  out.id = gBiomes[i].id;
  out.displayName = gBiomes[i].displayName;
  out.palette = gBiomes[i].palette;
  return out;
}

BiomeType biome_at(const World& world, int cellIndex) {
  if (cellIndex < 0 || cellIndex >= (int)world.biomeMap.size()) return BiomeType::TemperateGrassland;
  return static_cast<BiomeType>(world.biomeMap[cellIndex]);
}

std::string building_visual_variant_id(const World& world, const Building& building) {
  load_civilization_themes_once();
  if (building.team >= world.players.size()) return std::string("default_") + building_family_name(building.type);
  const std::string& civId = world.players[building.team].civilization.id;
  auto it = gThemes.find(civId);
  if (it == gThemes.end()) return std::string(civId) + "_" + building_family_name(building.type);
  const char* family = building_family_name(building.type);
  auto fit = it->second.familyToVariant.find(family);
  if (fit == it->second.familyToVariant.end()) return std::string(civId) + "_" + family;
  return fit->second;
}

uint64_t map_setup_hash(const World& w) {
  uint64_t h = kFNVOffset;
  hash_u32(h, static_cast<uint32_t>(w.worldPreset));
  for (float v : w.heightmap) hash_float(h, v);
  for (float v : w.fertility) hash_float(h, v);
  for (float v : w.temperatureMap) hash_float(h, v);
  for (float v : w.moistureMap) hash_float(h, v);
  for (uint8_t v : w.terrainClass) hash_u32(h, v);
  for (uint8_t v : w.coastClassMap) hash_u32(h, v);
  for (uint8_t v : w.biomeMap) hash_u32(h, v);
  for (uint8_t v : w.riverMap) hash_u32(h, v);
  for (uint8_t v : w.lakeMap) hash_u32(h, v);
  for (float v : w.resourceWeightMap) hash_float(h, v);
  for (int32_t v : w.landmassIdByCell) hash_u32(h, static_cast<uint32_t>(v + 1));
  for (const auto& c : w.startCandidates) { hash_u32(h, static_cast<uint32_t>(c.cell + 1)); hash_float(h, c.score); hash_u32(h, c.civBiasMask); }
  for (const auto& c : w.mythicCandidates) { hash_u32(h, static_cast<uint32_t>(c.siteType)); hash_u32(h, static_cast<uint32_t>(c.cell + 1)); hash_float(h, c.score); }
  for (const auto& c : w.cities) { hash_u32(h, c.id); hash_u32(h, c.team); hash_float(h, c.pos.x); hash_float(h, c.pos.y); }
  for (const auto& b : w.buildings) { hash_u32(h, b.id); hash_u32(h, b.team); hash_u32(h, (uint32_t)b.type); hash_float(h, b.pos.x); hash_float(h, b.pos.y); }
  for (const auto& u : w.units) { hash_u32(h, u.id); hash_u32(h, u.team); hash_u32(h, (uint32_t)u.type); hash_float(h, u.pos.x); hash_float(h, u.pos.y); }
  for (const auto& r : w.resourceNodes) { hash_u32(h, r.id); hash_u32(h, (uint32_t)r.type); hash_float(h, r.pos.x); hash_float(h, r.pos.y); hash_float(h, r.amount); }
  for (int32_t v : w.mountainRegionByCell) hash_u32(h, static_cast<uint32_t>(v + 1));
  for (const auto& mr : w.mountainRegions) {
    hash_u32(h, mr.id); hash_u32(h, mr.minX); hash_u32(h, mr.minY); hash_u32(h, mr.maxX); hash_u32(h, mr.maxY); hash_u32(h, mr.peakCell + 1); hash_u32(h, mr.centerCell + 1); hash_u32(h, mr.cellCount);
  }
  for (const auto& sd : w.surfaceDeposits) { hash_u32(h, sd.id); hash_u32(h, sd.regionId); hash_u32(h, (uint32_t)sd.mineral); hash_u32(h, sd.cell + 1); hash_float(h, sd.remaining); }
  for (const auto& dd : w.deepDeposits) { hash_u32(h, dd.id); hash_u32(h, dd.regionId); hash_u32(h, dd.nodeId); hash_u32(h, (uint32_t)dd.mineral); hash_u32(h, dd.cell + 1); hash_float(h, dd.remaining); }
  for (const auto& n : w.undergroundNodes) { hash_u32(h, n.id); hash_u32(h, n.regionId); hash_u32(h, (uint32_t)n.type); hash_u32(h, n.cell + 1); }
  for (const auto& e : w.undergroundEdges) { hash_u32(h, e.id); hash_u32(h, e.regionId); hash_u32(h, e.a); hash_u32(h, e.b); hash_u32(h, e.active?1u:0u); }
  for (const auto& o : w.objectives) { hash_u32(h, o.id); hash_u32(h, (uint32_t)o.state); }
  for (const auto& r : w.roads) { hash_u32(h, r.id); hash_u32(h, r.owner); hash_u32(h, (uint32_t)r.a.x); hash_u32(h, (uint32_t)r.a.y); hash_u32(h, (uint32_t)r.b.x); hash_u32(h, (uint32_t)r.b.y); hash_u32(h, r.quality); }
  for (const auto& n : w.railNodes) { hash_u32(h, n.id); hash_u32(h, n.owner); hash_u32(h, (uint32_t)n.type); hash_u32(h, (uint32_t)n.tile.x); hash_u32(h, (uint32_t)n.tile.y); hash_u32(h, n.networkId); hash_u32(h, n.active?1u:0u); }
  for (const auto& e : w.railEdges) { hash_u32(h, e.id); hash_u32(h, e.owner); hash_u32(h, e.aNode); hash_u32(h, e.bNode); hash_u32(h, e.quality); hash_u32(h, e.bridge?1u:0u); hash_u32(h, e.tunnel?1u:0u); hash_u32(h, e.disrupted?1u:0u); }
  return h;
}

uint64_t state_hash(const World& w) {
  uint64_t h = kFNVOffset;
  hash_u32(h, w.tick);
  hash_u32(h, static_cast<uint32_t>(w.units.size()));
  hash_u32(h, static_cast<uint32_t>(w.buildings.size()));
  for (const auto& u : w.units) { hash_u32(h, u.id); hash_float(h, u.hp); hash_float(h, u.pos.x); hash_float(h, u.pos.y); hash_u32(h, u.transportId); hash_u32(h, u.embarked ? 1u : 0u); hash_u32(h, (uint32_t)u.cargo.size()); for (uint32_t cid : u.cargo) hash_u32(h, cid); }
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
  hash_u32(h, w.logisticsRoadCount);
  hash_u32(h, w.logisticsTradeActiveCount);
  hash_u32(h, w.logisticsOperationIssuedCount);
  hash_u32(h, w.railNodeCount);
  hash_u32(h, w.railEdgeCount);
  hash_u32(h, w.activeRailNetworks);
  hash_u32(h, w.activeTrains);
  hash_u32(h, w.activeSupplyTrains);
  hash_u32(h, w.activeFreightTrains);
  hash_float(h, w.railThroughput);
  hash_u32(h, w.disruptedRailRoutes);
  hash_u32(h, w.suppliedUnits);
  hash_u32(h, w.lowSupplyUnits);
  hash_u32(h, w.outOfSupplyUnits);
  hash_u32(h, w.embarkEvents);
  hash_u32(h, w.disembarkEvents);
  hash_u32(h, w.navalCombatEvents);
  for (const auto& o : w.objectives) { hash_u32(h, o.id); hash_u32(h, (uint32_t)o.state); }
  for (const auto& l : w.objectiveLog) { hash_u32(h, l.tick); hash_u32(h, (uint32_t)l.text.size()); }
  hash_u32(h, (uint32_t)w.missionRuntime.status); hash_u32(h, (uint32_t)w.missionRuntime.briefingShown); hash_u32(h, (uint32_t)w.missionRuntime.resultTag.size());
  hash_u32(h, w.missionRuntime.firedTriggerCount); hash_u32(h, w.missionRuntime.scriptedActionCount);
  for (uint32_t id : w.missionRuntime.activeObjectives) hash_u32(h, id);
  for (const auto& l : w.missionRuntime.luaHookLog) hash_u32(h, (uint32_t)l.size());
  for (const auto& r : w.tradeRoutes) { hash_u32(h, r.id); hash_u32(h, r.team); hash_u32(h, r.fromCity); hash_u32(h, r.toCity); hash_u32(h, r.active ? 1u : 0u); hash_float(h, r.efficiency); hash_float(h, r.wealthPerTick); }
  for (const auto& o : w.operations) { hash_u32(h, o.id); hash_u32(h, o.team); hash_u32(h, (uint32_t)o.type); hash_float(h, o.target.x); hash_float(h, o.target.y); hash_u32(h, o.assignedTick); hash_u32(h, o.active ? 1u : 0u); }
  for (const auto& n : w.railNodes) { hash_u32(h, n.id); hash_u32(h, n.owner); hash_u32(h, (uint32_t)n.type); hash_u32(h, (uint32_t)n.tile.x); hash_u32(h, (uint32_t)n.tile.y); hash_u32(h, n.networkId); hash_u32(h, n.active?1u:0u); }
  for (const auto& e : w.railEdges) { hash_u32(h, e.id); hash_u32(h, e.owner); hash_u32(h, e.aNode); hash_u32(h, e.bNode); hash_u32(h, e.quality); hash_u32(h, e.bridge?1u:0u); hash_u32(h, e.tunnel?1u:0u); hash_u32(h, e.disrupted?1u:0u); }
  for (const auto& rn : w.railNetworks) { hash_u32(h, rn.id); hash_u32(h, rn.owner); hash_u32(h, rn.nodeCount); hash_u32(h, rn.edgeCount); hash_u32(h, rn.active?1u:0u); }
  for (const auto& t : w.trains) { hash_u32(h,t.id); hash_u32(h,t.owner); hash_u32(h,(uint32_t)t.type); hash_u32(h,(uint32_t)t.state); hash_u32(h,t.currentNode); hash_u32(h,t.destinationNode); hash_u32(h,t.currentEdge); hash_u32(h,t.routeCursor); hash_float(h,t.segmentProgress); hash_float(h,t.speed); hash_float(h,t.cargo); hash_float(h,t.capacity); hash_u32(h,t.lastRouteTick); hash_u32(h,(uint32_t)t.cargoType.size()); for (const auto& rs : t.route) { hash_u32(h, rs.edgeId); hash_u32(h, rs.toNode); } }
  hash_float(h, w.worldTension);
  for (const auto& r : w.diplomacy) hash_u32(h, static_cast<uint32_t>(r));
  for (const auto& t : w.treaties) {
    hash_u32(h, t.tradeAgreement ? 1u : 0u);
    hash_u32(h, t.openBorders ? 1u : 0u);
    hash_u32(h, t.alliance ? 1u : 0u);
    hash_u32(h, t.nonAggression ? 1u : 0u);
    hash_u32(h, t.lastChangedTick);
  }
  for (const auto& s : w.strategicPosture) hash_u32(h, static_cast<uint32_t>(s));
  for (const auto& op : w.espionageOps) {
    hash_u32(h, op.id); hash_u32(h, op.actor); hash_u32(h, op.target); hash_u32(h, static_cast<uint32_t>(op.type));
    hash_u32(h, op.startTick); hash_u32(h, op.durationTicks); hash_u32(h, static_cast<uint32_t>(op.state)); hash_u32(h, static_cast<uint32_t>(op.effectStrength));
  }
  hash_u32(h, w.diplomacyEventCount);
  hash_u32(h, w.postureChangeCount);
  for (const auto& a : w.airUnits) { hash_u32(h,a.id); hash_u32(h,a.team); hash_u32(h,(uint32_t)a.cls); hash_u32(h,(uint32_t)a.state); hash_float(h,a.pos.x); hash_float(h,a.pos.y); hash_float(h,a.missionTarget.x); hash_float(h,a.missionTarget.y); hash_float(h,a.hp); hash_float(h,a.speed); hash_u32(h,a.cooldownTicks); hash_u32(h,a.missionPerformed?1u:0u); }
  for (const auto& d : w.detectors) { hash_u32(h,d.id); hash_u32(h,d.team); hash_u32(h,(uint32_t)d.type); hash_float(h,d.pos.x); hash_float(h,d.pos.y); hash_float(h,d.radius); hash_u32(h,d.revealContactOnly?1u:0u); hash_u32(h,d.active?1u:0u); }
  for (const auto& st : w.strategicStrikes) { hash_u32(h,st.id); hash_u32(h,st.team); hash_u32(h,(uint32_t)st.type); hash_float(h,st.from.x); hash_float(h,st.from.y); hash_float(h,st.target.x); hash_float(h,st.target.y); hash_u32(h,st.prepTicksRemaining); hash_u32(h,st.travelTicksRemaining); hash_u32(h,st.cooldownTicks); hash_u32(h,st.interceptionState); hash_u32(h,st.launched?1u:0u); hash_u32(h,st.resolved?1u:0u); }
  for (const auto& dz : w.denialZones) { hash_u32(h,dz.id); hash_u32(h,dz.team); hash_float(h,dz.pos.x); hash_float(h,dz.pos.y); hash_float(h,dz.radius); hash_u32(h,dz.ticksRemaining); }
  for (uint8_t v : w.radarContactByPlayer) hash_u32(h, v);
  hash_u32(h, w.radarRevealEvents); hash_u32(h, w.strategicStrikeEvents); hash_u32(h, w.interceptionEvents); hash_u32(h, w.airMissionEvents);
  hash_u32(h, w.mountainRegionCount); hash_u32(h, w.surfaceDepositCount); hash_u32(h, w.deepDepositCount);
  hash_u32(h, w.activeMineShafts); hash_u32(h, w.activeTunnels); hash_u32(h, w.undergroundDepots); hash_float(h, w.undergroundYield);
  hash_u32(h, w.guardiansDiscovered); hash_u32(h, w.guardiansSpawned); hash_u32(h, w.guardiansJoined); hash_u32(h, w.guardiansKilled);
  hash_u32(h, w.hostileGuardianEvents); hash_u32(h, w.alliedGuardianEvents);
  for (const auto& s : w.guardianSites) {
    hash_u32(h, s.instanceId); hash_u32(h, (uint32_t)s.siteType); hash_u32(h, (uint32_t)s.guardianId.size());
    hash_float(h, s.pos.x); hash_float(h, s.pos.y); hash_u32(h, static_cast<uint32_t>(s.regionId + 1)); hash_u32(h, static_cast<uint32_t>(s.nodeId + 1));
    hash_u32(h, s.discovered ? 1u : 0u); hash_u32(h, s.alive ? 1u : 0u); hash_u32(h, s.owner == UINT16_MAX ? 0xFFFFu : s.owner);
    hash_u32(h, s.siteActive ? 1u : 0u); hash_u32(h, s.siteDepleted ? 1u : 0u); hash_u32(h, s.spawned ? 1u : 0u);
    hash_u32(h, s.behaviorState); hash_u32(h, s.cooldownTicks); hash_u32(h, s.oneShotUsed ? 1u : 0u); hash_u32(h, s.scenarioPlaced ? 1u : 0u);
  }
  for (const auto& d : w.deepDeposits) { hash_u32(h, d.id); hash_float(h, d.remaining); hash_u32(h, d.owner); hash_u32(h, d.active?1u:0u); }
  for (const auto& e : w.undergroundEdges) { hash_u32(h, e.id); hash_u32(h, e.owner); hash_u32(h, e.active?1u:0u); }
  for (uint8_t v : w.fog) hash_u32(h, v);
  for (uint8_t v : w.fogVisibilityByPlayer) hash_u32(h, v);
  for (uint8_t v : w.fogExploredByPlayer) hash_u32(h, v);
  for (uint8_t v : w.fogMaskByPlayer) hash_u32(h, v);
  return h;
}

} // namespace dom::sim
