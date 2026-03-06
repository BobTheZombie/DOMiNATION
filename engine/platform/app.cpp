#include "engine/platform/app.h"
#include "engine/core/time.h"
#include "engine/render/renderer.h"
#include "engine/sim/simulation.h"
#include "game/ai/simple_ai.h"
#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace {
struct CliOptions {
  bool headless{false};
  bool smoke{false};
  bool dumpHash{false};
  bool navDebug{false};
  bool flowVisualize{false};
  bool aiAttackEarly{false};
  bool aiAggressive{false};
  bool combatDebug{false};
  bool replayVerify{false};
  bool replaySummaryOnly{false};
  bool forceScoreVictory{false};
  bool forceWonderProgress{false};
  bool matchDebug{false};
  bool editor{false};
  bool listScenarios{false};
  uint32_t seed{1337};
  int ticks{-1};
  int mapW{128};
  int mapH{128};
  int timeLimitTicks{-1};
  int autosaveTick{-1};
  int replayStopTick{-1};
  float replaySpeed{1.0f};
  std::string recordReplayFile;
  std::string replayFile;
  std::string saveFile;
  std::string loadFile;
  std::string scenarioFile;
  std::string editorSaveFile{"scenario_editor_output.json"};
};

struct SelectionState {
  std::array<std::vector<uint32_t>, 9> controlGroups;
  std::array<uint32_t, 9> lastTapMs{};
  bool dragging{false};
  glm::vec2 dragStart{};
  glm::vec2 dragCurrent{};
  std::vector<uint32_t> dragHighlight;
};

bool parse_int(const std::string& s, int& out) { try { out = std::stoi(s); return true; } catch (...) { return false; } }
bool parse_float(const std::string& s, float& out) { char* e = nullptr; out = std::strtof(s.c_str(), &e); return e && *e == 0; }
bool parse_u32(const std::string& s, uint32_t& out) {
  try { unsigned long v = std::stoul(s); if (v > std::numeric_limits<uint32_t>::max()) return false; out = static_cast<uint32_t>(v); return true; }
  catch (...) { return false; }
}

bool parse_cli(int argc, char** argv, CliOptions& o) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--headless") o.headless = true;
    else if (a == "--smoke") o.smoke = true;
    else if (a == "--dump-hash") o.dumpHash = true;
    else if (a == "--nav-debug") o.navDebug = true;
    else if (a == "--flow-visualize") o.flowVisualize = true;
    else if (a == "--ai-attack-early") o.aiAttackEarly = true;
    else if (a == "--ai-aggressive") o.aiAggressive = true;
    else if (a == "--combat-debug") o.combatDebug = true;
    else if (a == "--match-debug") o.matchDebug = true;
    else if (a == "--editor") o.editor = true;
    else if (a == "--list-scenarios") o.listScenarios = true;
    else if (a == "--replay-verify") o.replayVerify = true;
    else if (a == "--replay-summary-only") o.replaySummaryOnly = true;
    else if (a == "--force-score-victory") o.forceScoreVictory = true;
    else if (a == "--force-wonder-progress") o.forceWonderProgress = true;
    else if (a == "--ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.ticks) || o.ticks < 0) return false; }
    else if (a == "--seed" && i + 1 < argc) { if (!parse_u32(argv[++i], o.seed)) return false; }
    else if (a == "--time-limit-ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.timeLimitTicks) || o.timeLimitTicks <= 0) return false; }
    else if (a == "--autosave-tick" && i + 1 < argc) { if (!parse_int(argv[++i], o.autosaveTick) || o.autosaveTick < 0) return false; }
    else if (a == "--replay-stop-tick" && i + 1 < argc) { if (!parse_int(argv[++i], o.replayStopTick) || o.replayStopTick < 0) return false; }
    else if (a == "--replay-speed" && i + 1 < argc) { float v = 1.0f; if (!parse_float(argv[++i], v) || v <= 0.0f) return false; o.replaySpeed = std::max(0.1f, v); }
    else if (a == "--record-replay" && i + 1 < argc) { o.recordReplayFile = argv[++i]; }
    else if (a == "--replay" && i + 1 < argc) { o.replayFile = argv[++i]; }
    else if (a == "--save" && i + 1 < argc) { o.saveFile = argv[++i]; }
    else if (a == "--load" && i + 1 < argc) { o.loadFile = argv[++i]; }
    else if (a == "--scenario" && i + 1 < argc) { o.scenarioFile = argv[++i]; }
    else if (a == "--editor-save" && i + 1 < argc) { o.editorSaveFile = argv[++i]; }
    else if (a == "--map-size" && i + 1 < argc) {
      std::string v = argv[++i]; auto xPos = v.find('x'); if (xPos == std::string::npos) return false;
      int w = 0, h = 0; if (!parse_int(v.substr(0, xPos), w) || !parse_int(v.substr(xPos + 1), h)) return false;
      if (w < 16 || h < 16 || w > 1024 || h > 1024) return false; o.mapW = w; o.mapH = h;
    } else { std::cerr << "Unknown or malformed argument: " << a << "\n"; return false; }
  }
  return true;
}

uint32_t first_building(const dom::sim::World& w, uint16_t team, dom::sim::BuildingType t) {
  for (const auto& b : w.buildings) if (b.team == team && b.type == t && !b.underConstruction) return b.id;
  return 0;
}

std::vector<uint32_t> collect_team_units(const dom::sim::World& world, uint16_t team, glm::vec2 a, glm::vec2 b) {
  float minX = std::min(a.x, b.x), maxX = std::max(a.x, b.x);
  float minY = std::min(a.y, b.y), maxY = std::max(a.y, b.y);
  std::vector<uint32_t> ids;
  for (const auto& u : world.units) {
    if (u.team != team) continue;
    if (u.pos.x >= minX && u.pos.x <= maxX && u.pos.y >= minY && u.pos.y <= maxY) ids.push_back(u.id);
  }
  return ids;
}

void apply_selection(dom::sim::World& world, std::vector<uint32_t>& selected, const std::vector<uint32_t>& ids) {
  selected = ids;
  for (auto& u : world.units) u.selected = std::find(selected.begin(), selected.end(), u.id) != selected.end();
}

glm::vec2 group_center(const dom::sim::World& world, const std::vector<uint32_t>& ids) {
  glm::vec2 c{0.0f, 0.0f};
  int n = 0;
  for (const auto& u : world.units) {
    if (std::find(ids.begin(), ids.end(), u.id) == ids.end()) continue;
    c += u.pos;
    ++n;
  }
  return n > 0 ? c / static_cast<float>(n) : c;
}

void update_drag_highlight(dom::sim::World& world, SelectionState& s, const dom::render::Camera& camera, int w, int h) {
  if (!s.dragging) return;
  if (glm::length(s.dragCurrent - s.dragStart) < 5.0f) { s.dragHighlight.clear(); return; }
  glm::vec2 wa = dom::render::screen_to_world(camera, w, h, s.dragStart);
  glm::vec2 wb = dom::render::screen_to_world(camera, w, h, s.dragCurrent);
  s.dragHighlight = collect_team_units(world, 0, wa, wb);
}


std::string victory_to_string(dom::sim::VictoryCondition c) {
  if (c == dom::sim::VictoryCondition::Conquest) return "conquest";
  if (c == dom::sim::VictoryCondition::Score) return "score";
  if (c == dom::sim::VictoryCondition::Wonder) return "wonder";
  return "none";
}
std::string cmd_type_to_string(dom::sim::ReplayCommandType t) {
  switch (t) {
    case dom::sim::ReplayCommandType::Move: return "move";
    case dom::sim::ReplayCommandType::Attack: return "attack";
    case dom::sim::ReplayCommandType::AttackMove: return "attack-move";
    case dom::sim::ReplayCommandType::PlaceBuilding: return "build-placement";
    case dom::sim::ReplayCommandType::QueueTrain: return "queue-train";
    case dom::sim::ReplayCommandType::QueueResearch: return "queue-research";
    case dom::sim::ReplayCommandType::CancelQueue: return "queue-cancel";
  }
  return "move";
}

dom::sim::ReplayCommandType string_to_cmd_type(const std::string& t) {
  if (t == "attack") return dom::sim::ReplayCommandType::Attack;
  if (t == "attack-move") return dom::sim::ReplayCommandType::AttackMove;
  if (t == "build-placement") return dom::sim::ReplayCommandType::PlaceBuilding;
  if (t == "queue-train") return dom::sim::ReplayCommandType::QueueTrain;
  if (t == "queue-research") return dom::sim::ReplayCommandType::QueueResearch;
  if (t == "queue-cancel") return dom::sim::ReplayCommandType::CancelQueue;
  return dom::sim::ReplayCommandType::Move;
}

dom::sim::BuildingType string_to_building(const std::string& v) {
  if (v == "CityCenter") return dom::sim::BuildingType::CityCenter;
  if (v == "Farm") return dom::sim::BuildingType::Farm;
  if (v == "LumberCamp") return dom::sim::BuildingType::LumberCamp;
  if (v == "Mine") return dom::sim::BuildingType::Mine;
  if (v == "Market") return dom::sim::BuildingType::Market;
  if (v == "Library") return dom::sim::BuildingType::Library;
  if (v == "Barracks") return dom::sim::BuildingType::Barracks;
  if (v == "Wonder") return dom::sim::BuildingType::Wonder;
  return dom::sim::BuildingType::House;
}
std::string building_to_string(dom::sim::BuildingType v) {
  switch (v) {
    case dom::sim::BuildingType::CityCenter: return "CityCenter";
    case dom::sim::BuildingType::House: return "House";
    case dom::sim::BuildingType::Farm: return "Farm";
    case dom::sim::BuildingType::LumberCamp: return "LumberCamp";
    case dom::sim::BuildingType::Mine: return "Mine";
    case dom::sim::BuildingType::Market: return "Market";
    case dom::sim::BuildingType::Library: return "Library";
    case dom::sim::BuildingType::Barracks: return "Barracks";
    case dom::sim::BuildingType::Wonder: return "Wonder";
  }
  return "House";
}


constexpr uint32_t kSaveSchemaVersion = 1;

nlohmann::json save_world_json(const dom::sim::World& w) {
  nlohmann::json j;
  j["schemaVersion"] = kSaveSchemaVersion;
  j["seed"] = w.seed;
  j["tick"] = w.tick;
  j["mapWidth"] = w.width;
  j["mapHeight"] = w.height;
  j["heightmap"] = w.heightmap;
  j["fertility"] = w.fertility;
  j["territoryOwner"] = w.territoryOwner;
  j["fog"] = w.fog;
  j["players"] = nlohmann::json::array();
  for (const auto& p : w.players) {
    j["players"].push_back({{"id", p.id}, {"age", (int)p.age}, {"resources", p.resources}, {"popUsed", p.popUsed}, {"popCap", p.popCap}, {"score", p.score}, {"alive", p.alive}, {"unitsLost", p.unitsLost}, {"buildingsLost", p.buildingsLost}, {"finalScore", p.finalScore}});
  }
  j["cities"] = nlohmann::json::array();
  for (const auto& c : w.cities) j["cities"].push_back({{"id", c.id}, {"team", c.team}, {"pos", {c.pos.x, c.pos.y}}, {"level", c.level}, {"capital", c.capital}});
  j["units"] = nlohmann::json::array();
  for (const auto& u : w.units) {
    j["units"].push_back({{"id", u.id}, {"team", u.team}, {"type", (int)u.type}, {"hp", u.hp}, {"attack", u.attack}, {"range", u.range}, {"speed", u.speed}, {"role", (int)u.role},
      {"attackType", (int)u.attackType}, {"preferredTargetRole", (int)u.preferredTargetRole}, {"vsRoleMultiplierPermille", u.vsRoleMultiplierPermille}, {"pos", {u.pos.x, u.pos.y}},
      {"renderPos", {u.renderPos.x, u.renderPos.y}}, {"target", {u.target.x, u.target.y}}, {"slotTarget", {u.slotTarget.x, u.slotTarget.y}}, {"moveDir", {u.moveDir.x, u.moveDir.y}},
      {"targetUnit", u.targetUnit}, {"moveOrder", u.moveOrder}, {"attackMoveOrder", u.attackMoveOrder}, {"targetLockTicks", u.targetLockTicks}, {"chaseTicks", u.chaseTicks},
      {"attackCooldownTicks", u.attackCooldownTicks}, {"lastTargetSwitchTick", u.lastTargetSwitchTick}, {"stuckTicks", u.stuckTicks}, {"orderPathLingerTicks", u.orderPathLingerTicks},
      {"hasMoveOrder", u.hasMoveOrder}, {"attackMove", u.attackMove}});
  }
  j["buildings"] = nlohmann::json::array();
  for (const auto& b : w.buildings) {
    nlohmann::json q = nlohmann::json::array();
    for (const auto& it : b.queue) q.push_back({{"kind", (int)it.kind}, {"unitType", (int)it.unitType}, {"remaining", it.remaining}, {"targetAge", it.targetAge}});
    j["buildings"].push_back({{"id", b.id}, {"team", b.team}, {"type", (int)b.type}, {"pos", {b.pos.x, b.pos.y}}, {"size", {b.size.x, b.size.y}}, {"underConstruction", b.underConstruction},
      {"buildProgress", b.buildProgress}, {"buildTime", b.buildTime}, {"hp", b.hp}, {"maxHp", b.maxHp}, {"queue", q}});
  }
  j["resourceNodes"] = nlohmann::json::array();
  for (const auto& r : w.resourceNodes) j["resourceNodes"].push_back({{"id", r.id}, {"type", (int)r.type}, {"pos", {r.pos.x, r.pos.y}}, {"amount", r.amount}, {"owner", r.owner}});
  j["triggerAreas"] = nlohmann::json::array();
  for (const auto& a : w.triggerAreas) j["triggerAreas"].push_back({{"id", a.id}, {"min", {a.min.x, a.min.y}}, {"max", {a.max.x, a.max.y}}});
  j["objectives"] = nlohmann::json::array();
  for (const auto& o : w.objectives) j["objectives"].push_back({{"id", o.id}, {"title", o.title}, {"text", o.text}, {"primary", o.primary}, {"state", (int)o.state}, {"owner", o.owner}});
  j["triggers"] = nlohmann::json::array();
  for (const auto& t : w.triggers) {
    nlohmann::json actions = nlohmann::json::array();
    for (const auto& a : t.actions) actions.push_back({{"type", (int)a.type}, {"text", a.text}, {"objectiveId", a.objectiveId}, {"objectiveState", (int)a.objectiveState}, {"player", a.player}, {"resources", a.resources}, {"spawnUnitType", (int)a.spawnUnitType}, {"spawnCount", a.spawnCount}, {"spawnPos", {a.spawnPos.x, a.spawnPos.y}}, {"winner", a.winner}, {"areaId", a.areaId}});
    j["triggers"].push_back({{"id", t.id}, {"once", t.once}, {"fired", t.fired}, {"condition", {{"type", (int)t.condition.type}, {"tick", t.condition.tick}, {"entityId", t.condition.entityId}, {"buildingType", (int)t.condition.buildingType}, {"areaId", t.condition.areaId}, {"player", t.condition.player}}}, {"actions", actions}});
  }
  j["objectiveLog"] = nlohmann::json::array();
  for (const auto& l : w.objectiveLog) j["objectiveLog"].push_back({{"tick", l.tick}, {"text", l.text}});
  j["match"] = {{"phase", (int)w.match.phase}, {"condition", (int)w.match.condition}, {"winner", w.match.winner}, {"endTick", w.match.endTick}, {"scoreTieBreak", w.match.scoreTieBreak}};
  j["config"] = {{"timeLimitTicks", w.config.timeLimitTicks}, {"wonderHoldTicks", w.config.wonderHoldTicks}, {"scoreResourceWeight", w.config.scoreResourceWeight},
    {"scoreUnitWeight", w.config.scoreUnitWeight}, {"scoreBuildingWeight", w.config.scoreBuildingWeight}, {"scoreAgeWeight", w.config.scoreAgeWeight}, {"scoreCapitalWeight", w.config.scoreCapitalWeight}, {"allowConquest", w.config.allowConquest}, {"allowScore", w.config.allowScore}, {"allowWonder", w.config.allowWonder}};
  j["triggerExecutionCount"] = w.triggerExecutionCount;
  j["objectiveStateChangeCount"] = w.objectiveStateChangeCount;
  j["wonder"] = {{"owner", w.wonder.owner}, {"heldTicks", w.wonder.heldTicks}};
  j["stateHash"] = dom::sim::state_hash(w);
  return j;
}

bool load_world_json(const nlohmann::json& j, dom::sim::World& w, std::string& err) {
  if (j.value("schemaVersion", 0u) != kSaveSchemaVersion) { err = "save schema mismatch"; return false; }
  w.seed = j.value("seed", 1337u);
  w.tick = j.value("tick", 0u);
  w.width = j.value("mapWidth", 128);
  w.height = j.value("mapHeight", 128);
  w.heightmap = j.at("heightmap").get<std::vector<float>>();
  w.fertility = j.at("fertility").get<std::vector<float>>();
  w.territoryOwner = j.at("territoryOwner").get<std::vector<uint16_t>>();
  w.fog = j.at("fog").get<std::vector<uint8_t>>();
  w.players.clear();
  for (const auto& jp : j.at("players")) {
    dom::sim::PlayerState p{};
    p.id = jp.value("id", 0u); p.age = static_cast<dom::sim::Age>(jp.value("age", 0));
    p.resources = jp.at("resources").get<decltype(p.resources)>(); p.popUsed = jp.value("popUsed", 0); p.popCap = jp.value("popCap", 0); p.score = jp.value("score", 0);
    p.alive = jp.value("alive", true); p.unitsLost = jp.value("unitsLost", 0u); p.buildingsLost = jp.value("buildingsLost", 0u); p.finalScore = jp.value("finalScore", 0);
    w.players.push_back(p);
  }
  w.cities.clear();
  for (const auto& jc : j.at("cities")) {
    dom::sim::City c{}; c.id = jc.value("id", 0u); c.team = jc.value("team", 0u); c.pos = {jc["pos"][0].get<float>(), jc["pos"][1].get<float>()}; c.level = jc.value("level", 1); c.capital = jc.value("capital", false); w.cities.push_back(c);
  }
  w.units.clear();
  for (const auto& ju : j.at("units")) {
    dom::sim::Unit u{}; u.id = ju.value("id", 0u); u.team = ju.value("team", 0u); u.type = static_cast<dom::sim::UnitType>(ju.value("type", 0)); u.hp = ju.value("hp", 0.0f);
    u.attack = ju.value("attack", 0.0f); u.range = ju.value("range", 0.0f); u.speed = ju.value("speed", 0.0f); u.role = static_cast<dom::sim::UnitRole>(ju.value("role", 0));
    u.attackType = static_cast<dom::sim::AttackType>(ju.value("attackType", 0)); u.preferredTargetRole = static_cast<dom::sim::UnitRole>(ju.value("preferredTargetRole", 0));
    u.vsRoleMultiplierPermille = ju.at("vsRoleMultiplierPermille").get<decltype(u.vsRoleMultiplierPermille)>(); u.pos = {ju["pos"][0].get<float>(), ju["pos"][1].get<float>()};
    u.renderPos = {ju["renderPos"][0].get<float>(), ju["renderPos"][1].get<float>()}; u.target = {ju["target"][0].get<float>(), ju["target"][1].get<float>()};
    u.slotTarget = {ju["slotTarget"][0].get<float>(), ju["slotTarget"][1].get<float>()}; u.moveDir = {ju["moveDir"][0].get<float>(), ju["moveDir"][1].get<float>()};
    u.targetUnit = ju.value("targetUnit", 0u); u.moveOrder = ju.value("moveOrder", 0u); u.attackMoveOrder = ju.value("attackMoveOrder", 0u); u.targetLockTicks = ju.value("targetLockTicks", 0);
    u.chaseTicks = ju.value("chaseTicks", 0); u.attackCooldownTicks = ju.value("attackCooldownTicks", 0); u.lastTargetSwitchTick = ju.value("lastTargetSwitchTick", 0); u.stuckTicks = ju.value("stuckTicks", 0);
    u.orderPathLingerTicks = ju.value("orderPathLingerTicks", 0); u.hasMoveOrder = ju.value("hasMoveOrder", false); u.attackMove = ju.value("attackMove", false); u.selected = false; w.units.push_back(u);
  }
  w.buildings.clear();
  for (const auto& jb : j.at("buildings")) {
    dom::sim::Building b{}; b.id = jb.value("id", 0u); b.team = jb.value("team", 0u); b.type = static_cast<dom::sim::BuildingType>(jb.value("type", 0));
    b.pos = {jb["pos"][0].get<float>(), jb["pos"][1].get<float>()}; b.size = {jb["size"][0].get<float>(), jb["size"][1].get<float>()}; b.underConstruction = jb.value("underConstruction", false);
    b.buildProgress = jb.value("buildProgress", 0.0f); b.buildTime = jb.value("buildTime", 0.0f); b.hp = jb.value("hp", 0.0f); b.maxHp = jb.value("maxHp", 0.0f);
    for (const auto& jq : jb.at("queue")) { dom::sim::ProductionItem qi{}; qi.kind = static_cast<dom::sim::QueueKind>(jq.value("kind", 0)); qi.unitType = static_cast<dom::sim::UnitType>(jq.value("unitType", 0)); qi.remaining = jq.value("remaining", 0.0f); qi.targetAge = jq.value("targetAge", 0); b.queue.push_back(qi); }
    w.buildings.push_back(b);
  }
  w.resourceNodes.clear();
  if (j.contains("resourceNodes")) for (const auto& jr : j.at("resourceNodes")) { dom::sim::ResourceNode r{}; r.id = jr.value("id", 0u); r.type = static_cast<dom::sim::ResourceNodeType>(jr.value("type", 0)); r.pos = {jr["pos"][0].get<float>(), jr["pos"][1].get<float>()}; r.amount = jr.value("amount", 0.0f); r.owner = jr.value("owner", (uint16_t)UINT16_MAX); w.resourceNodes.push_back(r); }
  w.triggerAreas.clear();
  if (j.contains("triggerAreas")) for (const auto& ja : j.at("triggerAreas")) { dom::sim::TriggerArea a{}; a.id = ja.value("id", 0u); a.min = {ja["min"][0].get<float>(), ja["min"][1].get<float>()}; a.max = {ja["max"][0].get<float>(), ja["max"][1].get<float>()}; w.triggerAreas.push_back(a); }
  w.objectives.clear();
  if (j.contains("objectives")) for (const auto& jo : j.at("objectives")) { dom::sim::Objective o{}; o.id = jo.value("id", 0u); o.title = jo.value("title", ""); o.text = jo.value("text", ""); o.primary = jo.value("primary", true); o.state = static_cast<dom::sim::ObjectiveState>(jo.value("state", 0)); o.owner = jo.value("owner", (uint16_t)UINT16_MAX); w.objectives.push_back(o); }
  w.triggers.clear();
  if (j.contains("triggers")) for (const auto& jt : j.at("triggers")) { dom::sim::Trigger t{}; t.id = jt.value("id", 0u); t.once = jt.value("once", true); t.fired = jt.value("fired", false); const auto& jcnd = jt.at("condition"); t.condition.type = static_cast<dom::sim::TriggerType>(jcnd.value("type", 0)); t.condition.tick = jcnd.value("tick", 0u); t.condition.entityId = jcnd.value("entityId", 0u); t.condition.buildingType = static_cast<dom::sim::BuildingType>(jcnd.value("buildingType", 0)); t.condition.areaId = jcnd.value("areaId", 0u); t.condition.player = jcnd.value("player", (uint16_t)UINT16_MAX); for (const auto& ja : jt.at("actions")) { dom::sim::TriggerAction a{}; a.type = static_cast<dom::sim::TriggerActionType>(ja.value("type", 0)); a.text = ja.value("text", ""); a.objectiveId = ja.value("objectiveId", 0u); a.objectiveState = static_cast<dom::sim::ObjectiveState>(ja.value("objectiveState", 0)); a.player = ja.value("player", (uint16_t)UINT16_MAX); a.resources = ja.at("resources").get<decltype(a.resources)>(); a.spawnUnitType = static_cast<dom::sim::UnitType>(ja.value("spawnUnitType", 0)); a.spawnCount = ja.value("spawnCount", 0u); a.spawnPos = {ja["spawnPos"][0].get<float>(), ja["spawnPos"][1].get<float>()}; a.winner = ja.value("winner", 0u); a.areaId = ja.value("areaId", 0u); t.actions.push_back(a);} w.triggers.push_back(t);} 
  w.objectiveLog.clear();
  if (j.contains("objectiveLog")) for (const auto& jl : j.at("objectiveLog")) { w.objectiveLog.push_back({jl.value("tick", 0u), jl.value("text", "")}); }
  const auto& jm = j.at("match");
  w.match.phase = static_cast<dom::sim::MatchPhase>(jm.value("phase", 0)); w.match.condition = static_cast<dom::sim::VictoryCondition>(jm.value("condition", 0));
  w.match.winner = jm.value("winner", 0u); w.match.endTick = jm.value("endTick", 0u); w.match.scoreTieBreak = jm.value("scoreTieBreak", false);
  const auto& jc = j.at("config");
  w.config.timeLimitTicks = jc.value("timeLimitTicks", w.config.timeLimitTicks); w.config.wonderHoldTicks = jc.value("wonderHoldTicks", w.config.wonderHoldTicks);
  w.config.scoreResourceWeight = jc.value("scoreResourceWeight", w.config.scoreResourceWeight); w.config.scoreUnitWeight = jc.value("scoreUnitWeight", w.config.scoreUnitWeight);
  w.config.scoreBuildingWeight = jc.value("scoreBuildingWeight", w.config.scoreBuildingWeight); w.config.scoreAgeWeight = jc.value("scoreAgeWeight", w.config.scoreAgeWeight); w.config.scoreCapitalWeight = jc.value("scoreCapitalWeight", w.config.scoreCapitalWeight); w.config.allowConquest = jc.value("allowConquest", true); w.config.allowScore = jc.value("allowScore", true); w.config.allowWonder = jc.value("allowWonder", true);
  w.triggerExecutionCount = j.value("triggerExecutionCount", 0u);
  w.objectiveStateChangeCount = j.value("objectiveStateChangeCount", 0u);
  w.wonder.owner = j.at("wonder").value("owner", UINT16_MAX); w.wonder.heldTicks = j.at("wonder").value("heldTicks", 0u);
  dom::sim::on_authoritative_state_loaded(w);
  const uint64_t expected = j.value("stateHash", 0ull);
  if (expected != 0ull && dom::sim::state_hash(w) != expected) { err = "save hash mismatch"; return false; }
  return true;
}

bool save_world_file(const std::string& path, const dom::sim::World& w) {
  std::ofstream of(path);
  if (!of.good()) return false;
  of << save_world_json(w).dump(2) << "\n";  return true;
}

void apply_replay_command(dom::sim::World& w, const dom::sim::ReplayCommand& c) {
  using namespace dom::sim;
  if (c.type == ReplayCommandType::Move) issue_move(w, c.team, c.ids, c.target);
  else if (c.type == ReplayCommandType::Attack) issue_attack(w, c.team, c.ids, c.enemy);
  else if (c.type == ReplayCommandType::AttackMove) issue_attack_move(w, c.team, c.ids, c.target);
  else if (c.type == ReplayCommandType::PlaceBuilding) { start_build_placement(w, c.team, c.buildingType); update_build_placement(w, c.team, c.target); confirm_build_placement(w, c.team); }
  else if (c.type == ReplayCommandType::QueueTrain) enqueue_train_unit(w, c.team, c.buildingId, c.unitType);
  else if (c.type == ReplayCommandType::QueueResearch) enqueue_age_research(w, c.team, c.buildingId);
  else if (c.type == ReplayCommandType::CancelQueue) cancel_queue_item(w, c.team, c.buildingId, c.queueIndex);
}

int run_headless(const CliOptions& o) {
  dom::sim::set_nav_debug(o.navDebug);
  dom::ai::set_attack_early(o.aiAttackEarly);
  dom::ai::set_aggressive(o.aiAggressive);
  dom::sim::set_combat_debug(o.combatDebug);

  nlohmann::json replay;
  std::vector<dom::sim::ReplayCommand> replayCommands;
  uint64_t recordedExpectedHash = 0;
  int replayTotalTicks = 600;

  dom::sim::World world;
  if (!o.loadFile.empty()) {
    std::ifstream in(o.loadFile);
    if (!in.good()) { std::cerr << "Save file not found: " << o.loadFile << "\n"; return 61; }
    nlohmann::json inSave; in >> inSave;
    std::string err;
    if (!load_world_json(inSave, world, err)) { std::cerr << "Failed to load save: " << err << "\n"; return 62; }
    std::cout << "LOAD_RESULT path=" << o.loadFile << " tick=" << world.tick << " ok=1\n";
  } else if (!o.scenarioFile.empty()) {
    std::string err;
    if (!dom::sim::load_scenario_file(world, o.scenarioFile, o.seed, err)) { std::cerr << "Failed to load scenario: " << err << "\n"; return 64; }
    std::cout << "SCENARIO_LOAD path=" << o.scenarioFile << " ok=1\n";
  } else if (!o.replayFile.empty()) {
    std::ifstream in(o.replayFile);
    if (!in.good()) { std::cerr << "Replay file not found: " << o.replayFile << "\n"; return 31; }
    in >> replay;
    world.width = replay.value("mapWidth", 128);
    world.height = replay.value("mapHeight", 128);
    const uint32_t seed = replay.value("seed", 1337u);
    dom::sim::initialize_world(world, seed);
    world.config.timeLimitTicks = replay.value("timeLimitTicks", world.config.timeLimitTicks);
    for (const auto& c : replay["commands"]) {
      dom::sim::ReplayCommand cmd{};
      cmd.type = string_to_cmd_type(c.value("type", "move"));
      cmd.tick = c.value("tick", 0u);
      cmd.team = c.value("team", 0u);
      if (c.contains("ids")) cmd.ids = c["ids"].get<std::vector<uint32_t>>();
      if (c.contains("target")) cmd.target = {c["target"][0].get<float>(), c["target"][1].get<float>()};
      cmd.enemy = c.value("enemy", 0u);
      cmd.buildingId = c.value("buildingId", 0u);
      cmd.unitType = static_cast<dom::sim::UnitType>(c.value("unitType", 0));
      cmd.buildingType = string_to_building(c.value("buildingType", "House"));
      cmd.queueIndex = c.value("queueIndex", 0u);
      replayCommands.push_back(cmd);
    }
    recordedExpectedHash = replay.value("expectedFinalHash", 0ull);
    replayTotalTicks = replay.value("totalTicks", 600);
  } else {
    world.width = o.mapW;
    world.height = o.mapH;
    dom::sim::initialize_world(world, o.seed);
    if (o.timeLimitTicks > 0) world.config.timeLimitTicks = static_cast<uint32_t>(o.timeLimitTicks);
    if (o.forceScoreVictory) world.config.wonderHoldTicks = std::numeric_limits<uint32_t>::max();
  }

  const uint64_t baselineHash = dom::sim::map_setup_hash(world);
  if (o.smoke && o.replayFile.empty() && o.loadFile.empty() && o.scenarioFile.empty()) {
    dom::sim::World second; second.width = world.width; second.height = world.height; dom::sim::initialize_world(second, o.seed);
    if (baselineHash != dom::sim::map_setup_hash(second)) { std::cerr << "Smoke failure: map hash mismatch for identical seed\n"; return 2; }
  }

  std::vector<uint8_t> minimap;
  dom::render::generate_minimap_image(world, 256, minimap);
  if (o.smoke && minimap.empty()) { std::cerr << "Smoke failure: minimap generation failed\n"; return 11; }

  const int requestedTicks = o.ticks >= 0 ? o.ticks : (!o.replayFile.empty() ? replayTotalTicks : 600);
  const uint32_t stopTick = o.replayStopTick >= 0 ? (uint32_t)o.replayStopTick : (uint32_t)requestedTicks;
  size_t replayIdx = 0;
  std::vector<dom::sim::ReplayCommand> recorded;
  bool autosaved = false;
  while (world.tick < stopTick) {
    if (o.replayFile.empty()) {
      if (dom::sim::gameplay_orders_allowed(world)) {
        dom::ai::update_simple_ai(world, 0);
        dom::ai::update_simple_ai(world, 1);
      }
    } else {
      while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) {
        apply_replay_command(world, replayCommands[replayIdx]);
        ++replayIdx;
      }
    }
    dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);

    if (!autosaved && !o.saveFile.empty() && o.autosaveTick >= 0 && world.tick >= (uint32_t)o.autosaveTick) {
      const uint64_t saveHash = dom::sim::state_hash(world);
      if (!save_world_file(o.saveFile, world)) { std::cerr << "Failed to write save: " << o.saveFile << "\n"; return 63; }
      std::cout << "SAVE_RESULT path=" << o.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
      autosaved = true;
    }

    std::vector<dom::sim::ReplayCommand> drained;
    dom::sim::consume_replay_commands(drained);
    recorded.insert(recorded.end(), drained.begin(), drained.end());

    if (world.match.phase == dom::sim::MatchPhase::Postmatch && (o.smoke || !o.replayFile.empty() || o.replaySummaryOnly)) break;
  }

  if (!o.saveFile.empty() && !autosaved) {
    const uint64_t saveHash = dom::sim::state_hash(world);
    if (!save_world_file(o.saveFile, world)) { std::cerr << "Failed to write save: " << o.saveFile << "\n"; return 63; }
    std::cout << "SAVE_RESULT path=" << o.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
  }

  uint64_t finalHash = dom::sim::state_hash(world);
  if (o.replayVerify) {
    if (finalHash != recordedExpectedHash) {
      std::cout << "REPLAY_VERIFY failed expected=" << recordedExpectedHash << " actual=" << finalHash << "\n";
      std::cout << "REPLAY_RESULT tick=" << world.tick << " verify=fail hash=" << finalHash << "\n";
      return 41;
    }
    std::cout << "REPLAY_VERIFY success expected=" << recordedExpectedHash << " actual=" << finalHash << "\n";
  }
  if (!o.replayFile.empty()) std::cout << "REPLAY_RESULT tick=" << world.tick << " verify=" << (o.replayVerify ? "ok" : "skip") << " hash=" << finalHash << "\n";

  if (!o.recordReplayFile.empty()) {
    nlohmann::json out;
    out["schemaVersion"] = 1;
    out["seed"] = world.seed;
    out["mapWidth"] = world.width;
    out["mapHeight"] = world.height;
    out["timeLimitTicks"] = world.config.timeLimitTicks;
    out["flags"] = {"smoke", o.smoke, "aiAttackEarly", o.aiAttackEarly, "aiAggressive", o.aiAggressive};
    out["contentHash"] = dom::sim::map_setup_hash(world);
    out["commands"] = nlohmann::json::array();
    for (const auto& c : recorded) {
      nlohmann::json jc;
      jc["tick"] = c.tick;
      jc["type"] = cmd_type_to_string(c.type);
      jc["team"] = c.team;
      if (!c.ids.empty()) jc["ids"] = c.ids;
      jc["target"] = {c.target.x, c.target.y};
      jc["enemy"] = c.enemy;
      jc["buildingId"] = c.buildingId;
      jc["unitType"] = static_cast<int>(c.unitType);
      jc["buildingType"] = building_to_string(c.buildingType);
      jc["queueIndex"] = c.queueIndex;
      out["commands"].push_back(jc);
    }
    out["expectedFinalHash"] = finalHash;
    out["totalTicks"] = world.tick;
    std::ofstream of(o.recordReplayFile);
    of << out.dump(2) << "\n";
    std::cout << "REPLAY_FILE path=" << o.recordReplayFile << "\n";
  }

  std::cout << "MATCH_RESULT winner=" << world.match.winner << " condition=" << victory_to_string(world.match.condition) << " ticks=" << world.match.endTick << "\n";
  for (const auto& p : world.players) {
    int unitsAlive = 0;
    int buildingsAlive = 0;
    for (const auto& u : world.units) if (u.team == p.id && u.hp > 0) ++unitsAlive;
    for (const auto& b : world.buildings) if (b.team == p.id && b.hp > 0 && !b.underConstruction) ++buildingsAlive;
    std::cout << "PLAYER_RESULT id=" << p.id << " score=" << p.finalScore << " unitsAlive=" << unitsAlive << " unitsLost=" << p.unitsLost << " buildingsAlive=" << buildingsAlive << " age=" << (int)p.age + 1 << "\n";
  }

  if (o.smoke && world.rejectedCommandCount != 0 && world.match.condition == dom::sim::VictoryCondition::None) { std::cerr << "Smoke failure: rejected commands before end\n"; return 52; }
  if (o.smoke && o.timeLimitTicks > 0 && world.match.condition == dom::sim::VictoryCondition::None) { std::cerr << "Smoke failure: match did not resolve with time limit\n"; return 51; }

  std::cout << "TRIGGER_RESULT count=" << world.triggerExecutionCount << " objectiveTransitions=" << world.objectiveStateChangeCount << " log=" << world.objectiveLog.size() << "\n";
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("trigger") != std::string::npos && world.triggerExecutionCount < 1) { std::cerr << "Smoke failure: trigger did not fire\n"; return 65; }
  if (o.dumpHash) {
    std::cout << "map_hash=" << baselineHash << "\n";
    std::cout << "state_hash=" << finalHash << "\n";
  }
  return 0;
}

} // namespace

int run_app(int argc, char** argv) {
  CliOptions opts; if (!parse_cli(argc, argv, opts)) return 1;
  if (opts.listScenarios) {
    namespace fs = std::filesystem;
    if (!fs::exists("scenarios")) { std::cout << "No scenarios directory\n"; return 0; }
    for (const auto& e : fs::directory_iterator("scenarios")) if (e.path().extension() == ".json") std::cout << e.path().string() << "\n";
    return 0;
  }
  if (opts.headless) return run_headless(opts);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_Window* window = SDL_CreateWindow("DOMiNATION RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) { SDL_Quit(); return 1; }
  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  if (!ctx) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
  SDL_GL_SetSwapInterval(1); dom::render::init_renderer();

  dom::sim::set_nav_debug(opts.navDebug);
  dom::ai::set_attack_early(opts.aiAttackEarly);
  dom::sim::World world;
  std::vector<dom::sim::ReplayCommand> replayCommands;
  bool replayMode = false;
  bool replayPaused = false;
  float replaySpeed = std::max(0.1f, opts.replaySpeed);
  size_t replayIdx = 0;
  if (!opts.scenarioFile.empty()) {
    std::string err;
    if (!dom::sim::load_scenario_file(world, opts.scenarioFile, opts.seed, err)) { std::cerr << "Failed to load scenario: " << err << "\n"; world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed); }
  } else {
    world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed);
  }
  if (!opts.loadFile.empty()) {
    std::ifstream in(opts.loadFile);
    if (in.good()) {
      nlohmann::json inSave; in >> inSave; std::string err;
      if (load_world_json(inSave, world, err)) std::cout << "LOAD_RESULT path=" << opts.loadFile << " tick=" << world.tick << " ok=1\n";
      else std::cerr << "Failed to load save: " << err << "\n";
    }
  }
  if (!opts.replayFile.empty()) {
    std::ifstream in(opts.replayFile);
    if (in.good()) {
      nlohmann::json replay; in >> replay;
      world.width = replay.value("mapWidth", world.width); world.height = replay.value("mapHeight", world.height);
      dom::sim::initialize_world(world, replay.value("seed", world.seed));
      world.config.timeLimitTicks = replay.value("timeLimitTicks", world.config.timeLimitTicks);
      for (const auto& c : replay["commands"]) {
        dom::sim::ReplayCommand cmd{}; cmd.type = string_to_cmd_type(c.value("type", "move")); cmd.tick = c.value("tick", 0u); cmd.team = c.value("team", 0u);
        if (c.contains("ids")) cmd.ids = c["ids"].get<std::vector<uint32_t>>();
        if (c.contains("target")) cmd.target = {c["target"][0].get<float>(), c["target"][1].get<float>()};
        cmd.enemy = c.value("enemy", 0u); cmd.buildingId = c.value("buildingId", 0u); cmd.unitType = static_cast<dom::sim::UnitType>(c.value("unitType", 0));
        cmd.buildingType = string_to_building(c.value("buildingType", "House")); cmd.queueIndex = c.value("queueIndex", 0u); replayCommands.push_back(cmd);
      }
      replayMode = true;
    }
  }
  dom::render::Camera camera;
  if (opts.flowVisualize) std::cout << "flow visualization requested (debug overlay path not wired in this slice)\n";
  std::vector<uint32_t> selected;
  SelectionState sel;
  bool editorMode = opts.editor;
  int editorTool = 0;
  uint16_t editorOwner = 0;

  auto selected_building = [&]() -> uint32_t {
    if (selected.empty()) return 0;
    for (const auto& u : world.units) if (u.id == selected[0] && u.type == dom::sim::UnitType::Worker) {
      float best = 99999.0f; uint32_t id = 0;
      for (const auto& b : world.buildings) if (b.team == 0 && !b.underConstruction) {
        float d = glm::length(b.pos - u.pos);
        if (d < best) { best = d; id = b.id; }
      }
      return id;
    }
    return first_building(world, 0, dom::sim::BuildingType::CityCenter);
  };

  bool running = true; Uint64 prev = SDL_GetPerformanceCounter(); float accum = 0.0f;
  while (running) {
    Uint64 now = SDL_GetPerformanceCounter(); float frameDt = (now - prev) / static_cast<float>(SDL_GetPerformanceFrequency()); prev = now; accum += frameDt;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_F9) editorMode = !editorMode;
        if (editorMode && e.key.keysym.sym == SDLK_TAB) editorTool = (editorTool + 1) % 6;
        if (editorMode && e.key.keysym.sym == SDLK_o) editorOwner = (uint16_t)((editorOwner + 1) % std::max<size_t>(1, world.players.size()));
        if (editorMode && e.key.keysym.sym == SDLK_F5) { std::string err; if (dom::sim::save_scenario_file(opts.editorSaveFile, world, err)) std::cout << "SCENARIO_SAVE path=" << opts.editorSaveFile << "\n"; else std::cerr << "SCENARIO_SAVE failed: " << err << "\n"; }
        SDL_Keymod mod = SDL_GetModState();
        if (e.key.keysym.sym == SDLK_g) dom::sim::toggle_god_mode(world);
        if (replayMode) {
          if (e.key.keysym.sym == SDLK_SPACE) replayPaused = !replayPaused;
          if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS) replaySpeed = std::min(16.0f, replaySpeed * 2.0f);
          if (e.key.keysym.sym == SDLK_MINUS) replaySpeed = std::max(0.25f, replaySpeed * 0.5f);
          if (e.key.keysym.sym == SDLK_RIGHT && replayPaused) {
            while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) { apply_replay_command(world, replayCommands[replayIdx]); ++replayIdx; }
            dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
          }
          if (e.key.keysym.sym == SDLK_LEFT && replayPaused) {
            uint32_t target = world.tick > 20 ? world.tick - 20 : 0;
            dom::sim::initialize_world(world, world.seed); replayIdx = 0;
            while (world.tick < target) {
              while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) { apply_replay_command(world, replayCommands[replayIdx]); ++replayIdx; }
              dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
            }
          }
          if (e.key.keysym.sym == SDLK_LEFTBRACKET || e.key.keysym.sym == SDLK_RIGHTBRACKET || e.key.keysym.sym == SDLK_r) {
            int delta = (e.key.keysym.sym == SDLK_LEFTBRACKET ? -200 : (e.key.keysym.sym == SDLK_RIGHTBRACKET ? 200 : -1000000000));
            uint32_t target = e.key.keysym.sym == SDLK_r ? 0u : static_cast<uint32_t>(std::max(0, (int)world.tick + delta));
            dom::sim::initialize_world(world, world.seed); replayIdx = 0;
            while (world.tick < target) {
              while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) { apply_replay_command(world, replayCommands[replayIdx]); ++replayIdx; }
              dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
            }
          }
        }
        if (e.key.keysym.sym == SDLK_F1) dom::render::toggle_territory_overlay();
        if (e.key.keysym.sym == SDLK_F2) dom::render::toggle_border_overlay();
        if (e.key.keysym.sym == SDLK_F3) dom::render::toggle_fog_overlay();
        if (e.key.keysym.sym == SDLK_m) dom::render::toggle_minimap();
        if (e.key.keysym.sym == SDLK_b) { world.uiBuildMenu = !world.uiBuildMenu; world.uiTrainMenu = false; world.uiResearchMenu = false; }
        if (e.key.keysym.sym == SDLK_t) { world.uiTrainMenu = !world.uiTrainMenu; world.uiBuildMenu = false; world.uiResearchMenu = false; }
        if (!replayMode && e.key.keysym.sym == SDLK_r) { world.uiResearchMenu = !world.uiResearchMenu; world.uiBuildMenu = false; world.uiTrainMenu = false; }
        if (e.key.keysym.sym == SDLK_ESCAPE) dom::sim::cancel_build_placement(world);

        auto group_index = [&](SDL_Keycode k) -> int {
          if (k >= SDLK_1 && k <= SDLK_9) return static_cast<int>(k - SDLK_1);
          if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return static_cast<int>(k - SDLK_KP_1);
          return -1;
        };
        int gi = group_index(e.key.keysym.sym);

        if (world.uiBuildMenu) {
          if (e.key.keysym.sym == SDLK_1) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::House);
          if (e.key.keysym.sym == SDLK_2) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Farm);
          if (e.key.keysym.sym == SDLK_3) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::LumberCamp);
          if (e.key.keysym.sym == SDLK_4) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Mine);
          if (e.key.keysym.sym == SDLK_5) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Market);
          if (e.key.keysym.sym == SDLK_6) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Library);
          if (e.key.keysym.sym == SDLK_7) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Barracks);
          if (e.key.keysym.sym == SDLK_8) dom::sim::start_build_placement(world, 0, dom::sim::BuildingType::Wonder);
        } else if (world.uiTrainMenu) {
          uint32_t bid = selected_building();
          if (e.key.keysym.sym == SDLK_1 && bid) dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Worker);
          if (e.key.keysym.sym == SDLK_2 && bid) dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Infantry);
          if (e.key.keysym.sym == SDLK_3 && bid) dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Archer);
          if (e.key.keysym.sym == SDLK_4 && bid) dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Cavalry);
          if (e.key.keysym.sym == SDLK_5 && bid) dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Siege);
          if (e.key.keysym.sym == SDLK_BACKSPACE && bid) dom::sim::cancel_queue_item(world, 0, bid, 0);
        } else if (world.uiResearchMenu) {
          uint32_t bid = selected_building();
          if (e.key.keysym.sym == SDLK_1 && bid) dom::sim::enqueue_age_research(world, 0, bid);
        } else if (gi >= 0) {
          if ((mod & KMOD_CTRL) != 0) sel.controlGroups[gi] = selected;
          else {
            apply_selection(world, selected, sel.controlGroups[gi]);
            uint32_t nowMs = SDL_GetTicks();
            if (nowMs - sel.lastTapMs[gi] <= 350 && !selected.empty()) camera.center = group_center(world, selected);
            sel.lastTapMs[gi] = nowMs;
          }
        }
      }
      if (e.type == SDL_MOUSEWHEEL) camera.zoom = std::clamp(camera.zoom - e.wheel.y * (world.godMode ? 4.0f : 1.2f), 4.0f, world.godMode ? 160.0f : 35.0f);

      if (e.type == SDL_MOUSEMOTION && world.placementActive) {
        int w, h; SDL_GetWindowSize(window, &w, &h);
        auto wp = dom::render::screen_to_world(camera, w, h, {(float)e.motion.x, (float)e.motion.y});
        dom::sim::update_build_placement(world, 0, wp);
      }
      if (e.type == SDL_MOUSEMOTION && sel.dragging) {
        sel.dragCurrent = {(float)e.motion.x, (float)e.motion.y};
        int w, h; SDL_GetWindowSize(window, &w, &h);
        update_drag_highlight(world, sel, camera, w, h);
      }

      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int w, h; SDL_GetWindowSize(window, &w, &h);
        glm::vec2 screen{(float)e.button.x, (float)e.button.y};
        if (editorMode) {
          auto wp = dom::render::screen_to_world(camera, w, h, screen);
          if (editorTool == 0) { dom::sim::UnitType ut = dom::sim::UnitType::Infantry; if (world.units.size() % 2 == 0) ut = dom::sim::UnitType::Worker; world.units.push_back({(uint32_t)(world.units.empty()?1:world.units.back().id+1), editorOwner, ut, 100.0f, 8.0f, 2.5f, 4.0f, ut==dom::sim::UnitType::Worker?dom::sim::UnitRole::Worker:dom::sim::UnitRole::Infantry, dom::sim::AttackType::Melee, dom::sim::UnitRole::Infantry, {1000,1000,1000,1000,1000,1000}, wp, wp, wp, wp, {0,0},0,0,0,0,0,0,0,0,0,false,false,false}); }
          else if (editorTool == 1) { world.buildings.push_back({(uint32_t)(world.buildings.empty()?1:world.buildings.back().id+1), editorOwner, dom::sim::BuildingType::Barracks, wp, {3.0f,3.0f}, false, 1.0f, 20.0f, 1000.0f, 1000.0f, {}}); }
          else if (editorTool == 2) { world.resourceNodes.push_back({(uint32_t)(world.resourceNodes.empty()?1:world.resourceNodes.back().id+1), dom::sim::ResourceNodeType::Forest, wp, 1000.0f, UINT16_MAX}); }
          else if (editorTool == 3) { world.cities.push_back({(uint32_t)(world.cities.empty()?1:world.cities.back().id+1), editorOwner, wp, 1, true}); }
          else if (editorTool == 4) { glm::vec2 p2{wp.x+6.0f, wp.y+6.0f}; world.triggerAreas.push_back({(uint32_t)(world.triggerAreas.empty()?1:world.triggerAreas.back().id+1), wp, p2}); }
          else if (editorTool == 5) { if (!world.units.empty()) world.units.pop_back(); else if (!world.buildings.empty()) world.buildings.pop_back(); else if (!world.resourceNodes.empty()) world.resourceNodes.pop_back(); }
          dom::sim::on_authoritative_state_loaded(world);
        } else if (world.placementActive) {
          auto wp = dom::render::screen_to_world(camera, w, h, screen);
          dom::sim::update_build_placement(world, 0, wp);
          dom::sim::confirm_build_placement(world, 0);
        } else {
          glm::vec2 worldPos{};
          if (dom::render::minimap_screen_to_world(world, w, h, screen, worldPos)) {
            camera.center = worldPos;
          } else {
            sel.dragging = true;
            sel.dragStart = screen;
            sel.dragCurrent = screen;
            sel.dragHighlight.clear();
          }
        }
      }
      if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && sel.dragging && !world.placementActive) {
        int w, h; SDL_GetWindowSize(window, &w, &h);
        sel.dragging = false;
        if (glm::length(sel.dragCurrent - sel.dragStart) < 5.0f) {
          uint32_t pick = dom::render::pick_unit(world, camera, w, h, sel.dragCurrent);
          selected.clear();
          if (pick) selected.push_back(pick);
          apply_selection(world, selected, selected);
        } else {
          glm::vec2 wa = dom::render::screen_to_world(camera, w, h, sel.dragStart);
          glm::vec2 wb = dom::render::screen_to_world(camera, w, h, sel.dragCurrent);
          auto ids = collect_team_units(world, 0, wa, wb);
          apply_selection(world, selected, ids);
        }
        sel.dragHighlight.clear();
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        if (world.placementActive) dom::sim::cancel_build_placement(world);
        else if (!selected.empty()) {
          int w, h; SDL_GetWindowSize(window, &w, &h);
          auto target = dom::render::screen_to_world(camera, w, h, {(float)e.button.x, (float)e.button.y});
          dom::sim::issue_move(world, 0, selected, target);
        }
      }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float pan = frameDt * camera.zoom * 1.2f;
    if (keys[SDL_SCANCODE_W]) camera.center.y += pan;
    if (keys[SDL_SCANCODE_S]) camera.center.y -= pan;
    if (keys[SDL_SCANCODE_A]) camera.center.x -= pan;
    if (keys[SDL_SCANCODE_D]) camera.center.x += pan;

    while (accum >= dom::core::kSimDeltaSeconds) {
      if (replayMode) {
        if (!replayPaused) {
          while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) { apply_replay_command(world, replayCommands[replayIdx]); ++replayIdx; }
          dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
          accum -= dom::core::kSimDeltaSeconds / replaySpeed;
        } else {
          accum = 0.0f;
        }
      } else {
        dom::ai::update_simple_ai(world, 1);
        dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
        accum -= dom::core::kSimDeltaSeconds;
      }
      if (!opts.saveFile.empty() && opts.autosaveTick >= 0 && world.tick == (uint32_t)opts.autosaveTick) {
        const uint64_t saveHash = dom::sim::state_hash(world);
        if (save_world_file(opts.saveFile, world)) std::cout << "SAVE_RESULT path=" << opts.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
      }
    }

    int w, h; SDL_GetWindowSize(window, &w, &h);
    dom::render::draw(world, camera, w, h, sel.dragHighlight);
    std::string replayOverlay;
    if (replayMode) replayOverlay = "REPLAY tick=" + std::to_string(world.tick) + (replayPaused ? " paused" : " running") + " speed=" + std::to_string(replaySpeed) + "x";
    if (editorMode) replayOverlay += (replayOverlay.empty()?"":" | ") + ("EDITOR tool=" + std::to_string(editorTool) + " owner=" + std::to_string(editorOwner) + " [Tab tool][O owner][F5 save]");
    if (!world.objectiveLog.empty()) replayOverlay += (replayOverlay.empty()?"":" | ") + ("OBJ: " + world.objectiveLog.back().text);
    dom::ui::draw_hud(window, world, replayOverlay);
    SDL_GL_SwapWindow(window);
  }

  if (!opts.saveFile.empty()) {
    const uint64_t saveHash = dom::sim::state_hash(world);
    if (save_world_file(opts.saveFile, world)) std::cout << "SAVE_RESULT path=" << opts.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
  }

  SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
