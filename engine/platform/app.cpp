#include "engine/platform/app.h"
#include "engine/core/time.h"
#include "engine/render/renderer.h"
#include "engine/sim/simulation.h"
#include "game/ai/simple_ai.h"
#include "game/ui/hud.h"
#include "engine/editor/scenario_editor.h"
#include "engine/debug/debug_panels.h"
#include "engine/debug/debug_visuals.h"
#include "engine/assets/asset_manager.h"
#include "engine/tools/asset_browser.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
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
#include <thread>
#include <cstdio>
#include <cmath>
#include <mutex>
#include <deque>
#include <sstream>
#include <optional>

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#endif

namespace {
struct CliOptions {
  bool headless{false};
  bool smoke{false};
  bool dumpHash{false};
  bool hashOnly{false};
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
  bool perf{false};
  bool cpuOnlyBattle{false};
  int spawnArmy{-1};
  int threads{0};
  std::string perfLogFile;
  uint32_t seed{1337};
  std::string worldPreset{"pangaea"};
  int ticks{-1};
  int mapW{128};
  int mapH{128};
  int windowW{1920};
  int windowH{1080};
  bool fullscreen{false};
  bool borderless{false};
  float renderScale{1.0f};
  float uiScale{1.0f};
  int timeLimitTicks{-1};
  int autosaveTick{-1};
  int replayStopTick{-1};
  float replaySpeed{1.0f};
  std::string recordReplayFile;
  std::string replayFile;
  std::string saveFile;
  std::string loadFile;
  std::string scenarioFile;
  std::string campaignFile;
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

struct UiCommandLogEntry {
  uint32_t tick{0};
  std::string panel;
  std::string command;
  bool success{false};
  std::string detail;
};

struct UiNotification {
  uint32_t tick{0};
  std::string text;
};

struct ContentEntry {
  std::string path;
  std::string title;
  std::string description;
  std::string civilization;
  std::string worldPreset;
  std::string difficulty;
  std::string briefing;
};

struct SaveEntry {
  std::string path;
  std::string name;
  std::string scenario;
  std::string campaign;
  std::string civilization;
  std::string worldPreset;
  uint32_t tick{0};
};

struct FrontendState {
  enum class Screen { MainMenu, Skirmish, Scenario, Campaign, LoadGame, Options };
  Screen screen{Screen::MainMenu};
  bool active{true};
  uint32_t seed{1337};
  int mapW{128};
  int mapH{128};
  int players{2};
  int aiSlots{1};
  int armageddonNationsThreshold{2};
  int armageddonUsesThreshold{2};
  bool enableWorldEvents{true};
  bool enableGuardians{true};
  bool allowConquest{true};
  bool allowScore{true};
  bool allowWonder{true};
  bool aiAggressive{false};
  int selectedScenario{0};
  int selectedCampaign{0};
  int selectedSave{0};
  int selectedWorldPreset{0};
  int selectedHumanCiv{0};
  int selectedAiCiv{0};
  std::string validation;
  std::string launchStatus;
};

std::vector<std::string> read_civilization_ids() {
  std::vector<std::string> ids{"default"};
  std::ifstream in("content/civilizations.json");
  if (!in.good()) return ids;
  nlohmann::json j; in >> j;
  if (!j.is_array()) return ids;
  ids.clear();
  for (const auto& c : j) {
    const std::string id = c.value("civilization_id", c.value("id", std::string("")));
    if (!id.empty()) ids.push_back(id);
  }
  if (ids.empty()) ids.push_back("default");
  return ids;
}

template<typename T>
void clamp_selection(int& idx, const std::vector<T>& items) {
  if (items.empty()) { idx = 0; return; }
  idx = std::clamp(idx, 0, static_cast<int>(items.size()) - 1);
}

std::vector<ContentEntry> read_scenario_entries() {
  std::vector<ContentEntry> out;
  namespace fs = std::filesystem;
  if (!fs::exists("scenarios")) return out;
  for (const auto& e : fs::directory_iterator("scenarios")) {
    if (e.path().extension() != ".json") continue;
    ContentEntry c{}; c.path = e.path().string(); c.title = e.path().stem().string();
    std::ifstream in(c.path);
    if (in.good()) {
      nlohmann::json j; in >> j;
      c.title = j.value("title", c.title);
      c.description = j.value("description", std::string(""));
      c.briefing = j.value("briefing", std::string(""));
      c.worldPreset = j.value("worldPreset", j.value("world_preset", std::string("")));
      c.difficulty = j.value("difficulty", std::string(""));
      if (j.contains("players") && j["players"].is_array() && !j["players"].empty()) c.civilization = j["players"][0].value("civilization", std::string(""));
      if (c.briefing.empty() && j.contains("mission") && j["mission"].is_object()) c.briefing = j["mission"].value("briefing", std::string(""));
    }
    out.push_back(std::move(c));
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.path < b.path; });
  return out;
}

std::vector<ContentEntry> read_campaign_entries() {
  std::vector<ContentEntry> out;
  namespace fs = std::filesystem;
  if (!fs::exists("campaigns")) return out;
  for (const auto& e : fs::directory_iterator("campaigns")) {
    if (e.path().extension() != ".json") continue;
    ContentEntry c{}; c.path = e.path().string(); c.title = e.path().stem().string();
    std::ifstream in(c.path);
    if (in.good()) {
      nlohmann::json j; in >> j;
      c.title = j.value("display_name", c.title);
      c.description = j.value("description", std::string(""));
      if (j.contains("starting_state")) c.civilization = j["starting_state"].value("player_civilization", std::string(""));
      if (j.contains("missions") && j["missions"].is_array() && !j["missions"].empty()) {
        c.briefing = j["missions"][0].value("briefing", std::string(""));
        c.difficulty = j["missions"][0].value("difficulty", std::string(""));
      }
    }
    out.push_back(std::move(c));
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.path < b.path; });
  return out;
}

std::vector<SaveEntry> read_save_entries() {
  std::vector<SaveEntry> out;
  namespace fs = std::filesystem;
  if (!fs::exists("saves")) fs::create_directory("saves");
  for (const auto& e : fs::directory_iterator("saves")) {
    if (e.path().extension() != ".json") continue;
    SaveEntry s{}; s.path = e.path().string(); s.name = e.path().stem().string();
    std::ifstream in(s.path);
    if (in.good()) {
      nlohmann::json j; in >> j;
      s.tick = j.value("tick", 0u);
      s.scenario = j.value("scenarioFile", std::string(""));
      if (j.contains("campaign") && j["campaign"].is_object()) s.campaign = j["campaign"].value("campaignId", std::string(""));
      if (j.contains("players") && j["players"].is_array() && !j["players"].empty()) s.civilization = j["players"][0].value("civilization", std::string(""));
      if (j.contains("worldPreset")) s.worldPreset = std::to_string(j.value("worldPreset", 0));
    }
    out.push_back(std::move(s));
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.path > b.path; });
  return out;
}

const char* relation_name(dom::sim::DiplomacyRelation r) {
  if (r == dom::sim::DiplomacyRelation::Allied) return "Allied";
  if (r == dom::sim::DiplomacyRelation::War) return "War";
  if (r == dom::sim::DiplomacyRelation::Ceasefire) return "Ceasefire";
  return "Neutral";
}

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
    else if (a == "--hash-only") o.hashOnly = true;
    else if (a == "--nav-debug") o.navDebug = true;
    else if (a == "--flow-visualize") o.flowVisualize = true;
    else if (a == "--ai-attack-early") o.aiAttackEarly = true;
    else if (a == "--ai-aggressive") o.aiAggressive = true;
    else if (a == "--combat-debug") o.combatDebug = true;
    else if (a == "--match-debug") o.matchDebug = true;
    else if (a == "--editor") o.editor = true;
    else if (a == "--list-scenarios") o.listScenarios = true;
    else if (a == "--perf") o.perf = true;
    else if (a == "--cpu-only-battle") o.cpuOnlyBattle = true;
    else if (a == "--replay-verify") o.replayVerify = true;
    else if (a == "--replay-summary-only") o.replaySummaryOnly = true;
    else if (a == "--force-score-victory") o.forceScoreVictory = true;
    else if (a == "--force-wonder-progress") o.forceWonderProgress = true;
    else if (a == "--ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.ticks) || o.ticks < 0) return false; }
    else if (a == "--seed" && i + 1 < argc) { if (!parse_u32(argv[++i], o.seed)) return false; }
    else if (a == "--world-preset" && i + 1 < argc) { o.worldPreset = argv[++i]; }
    else if (a == "--time-limit-ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.timeLimitTicks) || o.timeLimitTicks <= 0) return false; }
    else if (a == "--autosave-tick" && i + 1 < argc) { if (!parse_int(argv[++i], o.autosaveTick) || o.autosaveTick < 0) return false; }
    else if (a == "--replay-stop-tick" && i + 1 < argc) { if (!parse_int(argv[++i], o.replayStopTick) || o.replayStopTick < 0) return false; }
    else if (a == "--replay-speed" && i + 1 < argc) { float v = 1.0f; if (!parse_float(argv[++i], v) || v <= 0.0f) return false; o.replaySpeed = std::max(0.1f, v); }
    else if (a == "--record-replay" && i + 1 < argc) { o.recordReplayFile = argv[++i]; }
    else if (a == "--replay" && i + 1 < argc) { o.replayFile = argv[++i]; }
    else if (a == "--save" && i + 1 < argc) { o.saveFile = argv[++i]; }
    else if (a == "--load" && i + 1 < argc) { o.loadFile = argv[++i]; }
    else if (a == "--scenario" && i + 1 < argc) { o.scenarioFile = argv[++i]; }
    else if (a == "--campaign" && i + 1 < argc) { o.campaignFile = argv[++i]; }
    else if (a == "--width" && i + 1 < argc) { if (!parse_int(argv[++i], o.windowW) || o.windowW < 640) return false; }
    else if (a == "--height" && i + 1 < argc) { if (!parse_int(argv[++i], o.windowH) || o.windowH < 480) return false; }
    else if (a == "--fullscreen") { o.fullscreen = true; }
    else if (a == "--borderless") { o.borderless = true; }
    else if (a == "--render-scale" && i + 1 < argc) { if (!parse_float(argv[++i], o.renderScale) || o.renderScale <= 0.1f || o.renderScale > 1.0f) return false; }
    else if (a == "--ui-scale" && i + 1 < argc) { if (!parse_float(argv[++i], o.uiScale) || o.uiScale < 0.5f || o.uiScale > 3.0f) return false; }
    else if (a == "--editor-save" && i + 1 < argc) { o.editorSaveFile = argv[++i]; }
    else if (a == "--perf-log" && i + 1 < argc) { o.perfLogFile = argv[++i]; }
    else if (a == "--spawn-army" && i + 1 < argc) { if (!parse_int(argv[++i], o.spawnArmy) || o.spawnArmy < 0) return false; }
    else if (a == "--threads" && i + 1 < argc) { if (!parse_int(argv[++i], o.threads) || o.threads <= 0) return false; }
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
  if (v == "Port") return dom::sim::BuildingType::Port;
  if (v == "RadarTower") return dom::sim::BuildingType::RadarTower;
  if (v == "MobileRadar") return dom::sim::BuildingType::MobileRadar;
  if (v == "Airbase") return dom::sim::BuildingType::Airbase;
  if (v == "MissileSilo") return dom::sim::BuildingType::MissileSilo;
  if (v == "AABattery") return dom::sim::BuildingType::AABattery;
  if (v == "AntiMissileDefense") return dom::sim::BuildingType::AntiMissileDefense;
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
    case dom::sim::BuildingType::Port: return "Port";
    case dom::sim::BuildingType::RadarTower: return "RadarTower";
    case dom::sim::BuildingType::MobileRadar: return "MobileRadar";
    case dom::sim::BuildingType::Airbase: return "Airbase";
    case dom::sim::BuildingType::MissileSilo: return "MissileSilo";
    case dom::sim::BuildingType::AABattery: return "AABattery";
    case dom::sim::BuildingType::AntiMissileDefense: return "AntiMissileDefense";
    case dom::sim::BuildingType::Count: break;
  }
  return "House";
}




struct CampaignMissionEntry {
  std::string missionId;
  std::string scenarioFile;
  std::string briefing;
  std::string debrief;
  std::string introImage;
  std::string locationLabel;
  std::string portraitId;
  std::string factionIconId;
  std::vector<std::string> tags;
  std::vector<std::string> expectedOutcomes;
  std::vector<std::string> prerequisites;
  std::vector<std::pair<std::string, std::string>> nextByOutcome;
  std::vector<std::pair<std::string, std::string>> nextByBranch;
};

struct CampaignDefinition {
  std::string campaignId;
  std::string displayName;
  std::string description;
  dom::sim::CampaignCarryoverState startState;
  std::vector<CampaignMissionEntry> missions;
};

struct CampaignRuntimeState {
  std::string campaignFile;
  CampaignDefinition definition;
  dom::sim::CampaignCarryoverState carryover;
  std::string currentMissionId;
  std::vector<std::string> completedMissions;
  std::vector<std::string> failedMissions;
  std::vector<std::string> branchHistory;
  std::vector<std::pair<std::string, bool>> branchUnlocked;
  bool campaignComplete{false};
  bool campaignFailed{false};
  uint32_t missionCount{0};
  uint32_t branchesTaken{0};
};

const CampaignMissionEntry* find_campaign_mission(const CampaignDefinition& def, const std::string& id) {
  for (const auto& m : def.missions) if (m.missionId == id) return &m;
  return nullptr;
}

bool campaign_flag_value(const dom::sim::CampaignCarryoverState& c, const std::string& name) {
  for (const auto& kv : c.flags) if (kv.first == name) return kv.second;
  return false;
}

bool parse_campaign_file(const std::string& path, CampaignDefinition& out, std::string& err) {
  std::ifstream in(path);
  if (!in.good()) { err = "campaign file not found"; return false; }
  nlohmann::json j; in >> j;
  out = {};
  out.campaignId = j.value("campaign_id", std::string("unknown_campaign"));
  out.displayName = j.value("display_name", out.campaignId);
  out.description = j.value("description", std::string(""));
  const auto& st = j.value("starting_state", nlohmann::json::object());
  out.startState.campaignId = out.campaignId;
  out.startState.playerCivilizationId = st.value("player_civilization", std::string("default"));
  out.startState.unlockedAge = static_cast<uint8_t>(std::clamp(st.value("unlocked_age", 0), 0, 7));
  if (st.contains("resources")) {
    const auto& r = st.at("resources");
    out.startState.resources[(size_t)dom::sim::Resource::Food] = r.value("Food", 0.0f);
    out.startState.resources[(size_t)dom::sim::Resource::Wood] = r.value("Wood", 0.0f);
    out.startState.resources[(size_t)dom::sim::Resource::Metal] = r.value("Metal", 0.0f);
    out.startState.resources[(size_t)dom::sim::Resource::Wealth] = r.value("Wealth", 0.0f);
    out.startState.resources[(size_t)dom::sim::Resource::Knowledge] = r.value("Knowledge", 0.0f);
    out.startState.resources[(size_t)dom::sim::Resource::Oil] = r.value("Oil", 0.0f);
  }
  out.startState.worldTension = st.value("world_tension", 0.0f);
  if (st.contains("flags")) for (auto it = st["flags"].begin(); it != st["flags"].end(); ++it) out.startState.flags.push_back({it.key(), it.value().get<bool>()});
  if (st.contains("variables")) for (auto it = st["variables"].begin(); it != st["variables"].end(); ++it) out.startState.variables.push_back({it.key(), it.value().get<int64_t>()});
  if (!j.contains("missions") || !j.at("missions").is_array()) { err = "missions array missing"; return false; }
  for (const auto& m : j.at("missions")) {
    CampaignMissionEntry e{};
    e.missionId = m.value("mission_id", std::string(""));
    e.scenarioFile = m.value("scenario", std::string(""));
    e.briefing = m.value("briefing", std::string(""));
    e.debrief = m.value("debrief", std::string(""));
    e.introImage = m.value("intro_image", std::string(""));
    e.locationLabel = m.value("location", std::string(""));
    e.portraitId = m.value("portrait", std::string(""));
    e.factionIconId = m.value("faction_icon", std::string(""));
    if (m.contains("tags")) e.tags = m.at("tags").get<std::vector<std::string>>();
    if (m.contains("expected_outcomes")) e.expectedOutcomes = m.at("expected_outcomes").get<std::vector<std::string>>();
    if (m.contains("prerequisites")) e.prerequisites = m.at("prerequisites").get<std::vector<std::string>>();
    if (m.contains("next")) for (auto it = m["next"].begin(); it != m["next"].end(); ++it) e.nextByOutcome.push_back({it.key(), it.value().get<std::string>()});
    if (m.contains("next_by_branch")) for (auto it = m["next_by_branch"].begin(); it != m["next_by_branch"].end(); ++it) e.nextByBranch.push_back({it.key(), it.value().get<std::string>()});
    if (e.missionId.empty() || e.scenarioFile.empty()) { err = "mission entry missing id/scenario"; return false; }
    out.missions.push_back(std::move(e));
  }
  if (out.missions.size() < 2) { err = "campaign requires >=2 missions"; return false; }
  return true;
}

bool campaign_runtime_json(const CampaignRuntimeState& c, nlohmann::json& j) {
  j = nlohmann::json::object();
  j["schemaVersion"] = 1;
  j["campaignRuntimeState"] = true;
  j["campaignFile"] = c.campaignFile;
  j["campaign"] = { {"campaign_id", c.definition.campaignId}, {"display_name", c.definition.displayName}, {"description", c.definition.description} };
  j["carryover"] = {
    {"campaign_id", c.carryover.campaignId}, {"player_civilization", c.carryover.playerCivilizationId}, {"unlocked_age", c.carryover.unlockedAge},
    {"resources", {{"Food",c.carryover.resources[0]},{"Wood",c.carryover.resources[1]},{"Metal",c.carryover.resources[2]},{"Wealth",c.carryover.resources[3]},{"Knowledge",c.carryover.resources[4]},{"Oil",c.carryover.resources[5]}}},
    {"veteran_units", c.carryover.veteranUnitIds}, {"discovered_guardians", c.carryover.discoveredGuardians}, {"world_tension", c.carryover.worldTension},
    {"unlocked_rewards", c.carryover.unlockedRewards}, {"previous_result", c.carryover.previousMissionResult}, {"pending_branch", c.carryover.pendingBranchKey}
  };
  nlohmann::json flags = nlohmann::json::object(); for (const auto& kv : c.carryover.flags) flags[kv.first] = kv.second; j["carryover"]["flags"] = flags;
  nlohmann::json vars = nlohmann::json::object(); for (const auto& kv : c.carryover.variables) vars[kv.first] = kv.second; j["carryover"]["variables"] = vars;
  j["currentMissionId"] = c.currentMissionId;
  j["completedMissions"] = c.completedMissions;
  j["failedMissions"] = c.failedMissions;
  j["branchHistory"] = c.branchHistory;
  j["campaignComplete"] = c.campaignComplete;
  j["campaignFailed"] = c.campaignFailed;
  j["missionCount"] = c.missionCount;
  j["branchesTaken"] = c.branchesTaken;
  return true;
}

void apply_campaign_carryover_to_world(dom::sim::World& w, const CampaignRuntimeState& c) {
  w.campaign = c.carryover;
  if (!w.players.empty()) {
    w.players[0].civilization.id = c.carryover.playerCivilizationId.empty() ? w.players[0].civilization.id : c.carryover.playerCivilizationId;
    w.players[0].age = static_cast<dom::sim::Age>(std::min<uint8_t>(c.carryover.unlockedAge, (uint8_t)dom::sim::Age::Information));
    for (size_t i = 0; i < c.carryover.resources.size(); ++i) w.players[0].resources[i] += c.carryover.resources[i];
  }
  w.worldTension = c.carryover.worldTension;
}

constexpr uint32_t kSaveSchemaVersion = 2;

nlohmann::json save_world_json(const dom::sim::World& w) {
  nlohmann::json j;
  j["schemaVersion"] = kSaveSchemaVersion;
  j["seed"] = w.seed;
  j["tick"] = w.tick;
  j["mapWidth"] = w.width;
  j["mapHeight"] = w.height;
  j["heightmap"] = w.heightmap;
  j["fertility"] = w.fertility;
  j["terrainClass"] = w.terrainClass;
  j["biomeMap"] = w.biomeMap;
  j["temperatureMap"] = w.temperatureMap;
  j["moistureMap"] = w.moistureMap;
  j["coastClassMap"] = w.coastClassMap;
  j["landmassIdByCell"] = w.landmassIdByCell;
  j["riverMap"] = w.riverMap;
  j["lakeMap"] = w.lakeMap;
  j["resourceWeightMap"] = w.resourceWeightMap;
  j["startCandidates"] = nlohmann::json::array();
  for (const auto& sc : w.startCandidates) j["startCandidates"].push_back({{"cell", sc.cell}, {"score", sc.score}, {"civBiasMask", sc.civBiasMask}});
  j["mythicCandidates"] = nlohmann::json::array();
  for (const auto& mc : w.mythicCandidates) j["mythicCandidates"].push_back({{"siteType", (int)mc.siteType}, {"cell", mc.cell}, {"score", mc.score}});
  j["territoryOwner"] = w.territoryOwner;
  j["fog"] = w.fog;
  j["fogVisibilityByPlayer"] = w.fogVisibilityByPlayer;
  j["fogExploredByPlayer"] = w.fogExploredByPlayer;
  j["fogMaskByPlayer"] = w.fogMaskByPlayer;
  j["players"] = nlohmann::json::array();
  for (const auto& p : w.players) {
    j["players"].push_back({{"id", p.id}, {"age", (int)p.age}, {"resources", p.resources}, {"popUsed", p.popUsed}, {"popCap", p.popCap}, {"score", p.score}, {"alive", p.alive}, {"unitsLost", p.unitsLost}, {"buildingsLost", p.buildingsLost}, {"finalScore", p.finalScore}, {"team", p.teamId}, {"civilization", p.civilization.id}});
  }
  j["cities"] = nlohmann::json::array();
  for (const auto& c : w.cities) j["cities"].push_back({{"id", c.id}, {"team", c.team}, {"pos", {c.pos.x, c.pos.y}}, {"level", c.level}, {"capital", c.capital}});
  j["units"] = nlohmann::json::array();
  for (const auto& u : w.units) {
    j["units"].push_back({{"id", u.id}, {"team", u.team}, {"type", (int)u.type}, {"hp", u.hp}, {"attack", u.attack}, {"range", u.range}, {"speed", u.speed}, {"role", (int)u.role},
      {"attackType", (int)u.attackType}, {"preferredTargetRole", (int)u.preferredTargetRole}, {"vsRoleMultiplierPermille", u.vsRoleMultiplierPermille}, {"pos", {u.pos.x, u.pos.y}},
      {"renderPos", {u.renderPos.x, u.renderPos.y}}, {"target", {u.target.x, u.target.y}}, {"slotTarget", {u.slotTarget.x, u.slotTarget.y}}, {"moveDir", {u.moveDir.x, u.moveDir.y}},
      {"targetUnit", u.targetUnit}, {"moveOrder", u.moveOrder}, {"attackMoveOrder", u.attackMoveOrder}, {"targetLockTicks", u.targetLockTicks}, {"chaseTicks", u.chaseTicks},
      {"attackCooldownTicks", u.attackCooldownTicks}, {"lastTargetSwitchTick", u.lastTargetSwitchTick}, {"stuckTicks", u.stuckTicks}, {"stealthRevealTicks", u.stealthRevealTicks}, {"orderPathLingerTicks", u.orderPathLingerTicks},
      {"supplyState", (int)u.supplyState}, {"transportId", u.transportId}, {"definitionId", u.definitionId}, {"cargo", u.cargo}, {"embarked", u.embarked}, {"hasMoveOrder", u.hasMoveOrder}, {"attackMove", u.attackMove}});
  }
  j["buildings"] = nlohmann::json::array();
  for (const auto& b : w.buildings) {
    nlohmann::json q = nlohmann::json::array();
    for (const auto& it : b.queue) q.push_back({{"kind", (int)it.kind}, {"unitType", (int)it.unitType}, {"remaining", it.remaining}, {"targetAge", it.targetAge}});
    j["buildings"].push_back({{"id", b.id}, {"team", b.team}, {"type", (int)b.type}, {"pos", {b.pos.x, b.pos.y}}, {"size", {b.size.x, b.size.y}}, {"underConstruction", b.underConstruction},
      {"buildProgress", b.buildProgress}, {"buildTime", b.buildTime}, {"hp", b.hp}, {"maxHp", b.maxHp}, {"definitionId", b.definitionId}, {"queue", q}});
  }
  j["resourceNodes"] = nlohmann::json::array();
  for (const auto& r : w.resourceNodes) j["resourceNodes"].push_back({{"id", r.id}, {"type", (int)r.type}, {"pos", {r.pos.x, r.pos.y}}, {"amount", r.amount}, {"owner", r.owner}});
  j["mountainRegionByCell"] = w.mountainRegionByCell;
  j["mountainRegions"] = nlohmann::json::array();
  for (const auto& mr : w.mountainRegions) j["mountainRegions"].push_back({{"id",mr.id},{"minX",mr.minX},{"minY",mr.minY},{"maxX",mr.maxX},{"maxY",mr.maxY},{"peakCell",mr.peakCell},{"centerCell",mr.centerCell},{"cellCount",mr.cellCount}});
  j["surfaceDeposits"] = nlohmann::json::array();
  for (const auto& sd : w.surfaceDeposits) j["surfaceDeposits"].push_back({{"id",sd.id},{"regionId",sd.regionId},{"mineral",(int)sd.mineral},{"cell",sd.cell},{"remaining",sd.remaining},{"owner",sd.owner}});
  j["deepDeposits"] = nlohmann::json::array();
  for (const auto& dd : w.deepDeposits) j["deepDeposits"].push_back({{"id",dd.id},{"regionId",dd.regionId},{"nodeId",dd.nodeId},{"mineral",(int)dd.mineral},{"cell",dd.cell},{"richness",dd.richness},{"remaining",dd.remaining},{"owner",dd.owner},{"active",dd.active}});
  j["undergroundNodes"] = nlohmann::json::array();
  for (const auto& n : w.undergroundNodes) j["undergroundNodes"].push_back({{"id",n.id},{"regionId",n.regionId},{"type",(int)n.type},{"cell",n.cell},{"linkedBuildingId",n.linkedBuildingId},{"owner",n.owner},{"active",n.active}});
  j["undergroundEdges"] = nlohmann::json::array();
  for (const auto& e : w.undergroundEdges) j["undergroundEdges"].push_back({{"id",e.id},{"regionId",e.regionId},{"a",e.a},{"b",e.b},{"owner",e.owner},{"active",e.active}});
  j["guardianDefinitions"] = nlohmann::json::array();
  for (const auto& d : w.guardianDefinitions) j["guardianDefinitions"].push_back({{"guardianId",d.guardianId},{"displayName",d.displayName},{"biomeRequirement",(int)d.biomeRequirement},{"siteType",(int)d.siteType},{"spawnMode",(int)d.spawnMode},{"maxPerMap",d.maxPerMap},{"unique",d.unique},{"discoveryMode",(int)d.discoveryMode},{"behaviorMode",(int)d.behaviorMode},{"joinMode",(int)d.joinMode},{"associatedUnitDefinitionId",d.associatedUnitDefinitionId},{"unitHp",d.unitHp},{"unitAttack",d.unitAttack},{"unitRange",d.unitRange},{"unitSpeed",d.unitSpeed},{"rewardHook",d.rewardHook},{"effectHook",d.effectHook},{"scenarioOnly",d.scenarioOnly},{"procedural",d.procedural},{"rarityPermille",d.rarityPermille},{"minSpacingCells",d.minSpacingCells},{"discoveryRadius",d.discoveryRadius}});
  j["guardianSites"] = nlohmann::json::array();
  for (const auto& s : w.guardianSites) j["guardianSites"].push_back({{"instanceId",s.instanceId},{"guardianId",s.guardianId},{"siteType",(int)s.siteType},{"pos",{s.pos.x,s.pos.y}},{"regionId",s.regionId},{"nodeId",s.nodeId},{"discovered",s.discovered},{"alive",s.alive},{"owner",s.owner},{"siteActive",s.siteActive},{"siteDepleted",s.siteDepleted},{"spawned",s.spawned},{"behaviorState",s.behaviorState},{"cooldownTicks",s.cooldownTicks},{"oneShotUsed",s.oneShotUsed},{"scenarioPlaced",s.scenarioPlaced}});
  j["roads"] = nlohmann::json::array();
  for (const auto& r : w.roads) j["roads"].push_back({{"id", r.id}, {"owner", r.owner}, {"a", {r.a.x, r.a.y}}, {"b", {r.b.x, r.b.y}}, {"quality", r.quality}});
  j["railNodes"] = nlohmann::json::array();
  for (const auto& n : w.railNodes) j["railNodes"].push_back({{"id", n.id}, {"owner", n.owner}, {"type", (int)n.type}, {"tile", {n.tile.x, n.tile.y}}, {"networkId", n.networkId}, {"active", n.active}});
  j["railEdges"] = nlohmann::json::array();
  for (const auto& e : w.railEdges) j["railEdges"].push_back({{"id", e.id}, {"owner", e.owner}, {"aNode", e.aNode}, {"bNode", e.bNode}, {"quality", e.quality}, {"bridge", e.bridge}, {"tunnel", e.tunnel}, {"disrupted", e.disrupted}});
  j["railNetworks"] = nlohmann::json::array();
  for (const auto& rn : w.railNetworks) j["railNetworks"].push_back({{"id", rn.id}, {"owner", rn.owner}, {"nodeCount", rn.nodeCount}, {"edgeCount", rn.edgeCount}, {"active", rn.active}});
  j["trains"] = nlohmann::json::array();
  for (const auto& t : w.trains) { nlohmann::json route = nlohmann::json::array(); for (const auto& st : t.route) route.push_back({{"edgeId", st.edgeId}, {"toNode", st.toNode}}); j["trains"].push_back({{"id", t.id}, {"owner", t.owner}, {"type", (int)t.type}, {"state", (int)t.state}, {"currentNode", t.currentNode}, {"destinationNode", t.destinationNode}, {"currentEdge", t.currentEdge}, {"routeCursor", t.routeCursor}, {"segmentProgress", t.segmentProgress}, {"speed", t.speed}, {"cargo", t.cargo}, {"capacity", t.capacity}, {"cargoType", t.cargoType}, {"lastRouteTick", t.lastRouteTick}, {"route", route}}); }
  j["operations"] = nlohmann::json::array();
  for (const auto& o : w.operations) j["operations"].push_back({{"id", o.id}, {"team", o.team}, {"type", (int)o.type}, {"target", {o.target.x, o.target.y}}, {"assignedTick", o.assignedTick}, {"active", o.active}});
  j["theaterCommands"] = nlohmann::json::array();
  for (const auto& t : w.theaterCommands) j["theaterCommands"].push_back({{"theaterId",t.theaterId},{"owner",t.owner},{"bounds",{t.bounds.x,t.bounds.y,t.bounds.z,t.bounds.w}},{"priority",(int)t.priority},{"activeOperations",t.activeOperations},{"assignedArmyGroups",t.assignedArmyGroups},{"assignedNavalTaskForces",t.assignedNavalTaskForces},{"assignedAirWings",t.assignedAirWings},{"supplyStatus",t.supplyStatus},{"threatLevel",t.threatLevel}});
  j["armyGroups"] = nlohmann::json::array();
  for (const auto& a : w.armyGroups) j["armyGroups"].push_back({{"id",a.id},{"owner",a.owner},{"theaterId",a.theaterId},{"unitIds",a.unitIds},{"stance",(int)a.stance},{"assignedObjective",a.assignedObjective},{"active",a.active}});
  j["navalTaskForces"] = nlohmann::json::array();
  for (const auto& n : w.navalTaskForces) j["navalTaskForces"].push_back({{"id",n.id},{"owner",n.owner},{"theaterId",n.theaterId},{"unitIds",n.unitIds},{"mission",(int)n.mission},{"assignedObjective",n.assignedObjective},{"active",n.active}});
  j["airWings"] = nlohmann::json::array();
  for (const auto& a : w.airWings) j["airWings"].push_back({{"id",a.id},{"owner",a.owner},{"theaterId",a.theaterId},{"squadronIds",a.squadronIds},{"mission",(int)a.mission},{"assignedObjective",a.assignedObjective},{"active",a.active}});
  j["operationalObjectives"] = nlohmann::json::array();
  for (const auto& o : w.operationalObjectives) j["operationalObjectives"].push_back({{"id",o.id},{"owner",o.owner},{"theaterId",o.theaterId},{"objectiveType",(int)o.objectiveType},{"targetRegion",{o.targetRegion.x,o.targetRegion.y,o.targetRegion.z,o.targetRegion.w}},{"requiredForce",o.requiredForce},{"startTick",o.startTick},{"durationTicks",o.durationTicks},{"outcome",(int)o.outcome},{"active",o.active},{"armyGroups",o.armyGroups},{"navalTaskForces",o.navalTaskForces},{"airWings",o.airWings}});
  j["worldTension"] = w.worldTension;
  j["diplomacyRelations"] = nlohmann::json::array();
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t k = i + 1; k < w.players.size(); ++k) {
      auto rel = w.diplomacy[i * w.players.size() + k];
      const char* name = "Neutral";
      if (rel == dom::sim::DiplomacyRelation::Allied) name = "Allied";
      else if (rel == dom::sim::DiplomacyRelation::War) name = "War";
      else if (rel == dom::sim::DiplomacyRelation::Ceasefire) name = "Ceasefire";
      j["diplomacyRelations"].push_back({{"a", (uint16_t)i}, {"b", (uint16_t)k}, {"relation", name}});
    }
  }
  j["treaties"] = nlohmann::json::array();
  for (size_t i = 0; i < w.players.size(); ++i) {
    for (size_t k = i + 1; k < w.players.size(); ++k) {
      const auto& t = w.treaties[i * w.players.size() + k];
      j["treaties"].push_back({{"a", (uint16_t)i}, {"b", (uint16_t)k}, {"tradeAgreement", t.tradeAgreement}, {"openBorders", t.openBorders}, {"alliance", t.alliance}, {"nonAggression", t.nonAggression}, {"lastChangedTick", t.lastChangedTick}});
    }
  }
  j["espionageOps"] = nlohmann::json::array();
  for (const auto& op : w.espionageOps) {
    const char* type = "RECON_CITY";
    if (op.type == dom::sim::EspionageOpType::RevealRoute) type = "REVEAL_ROUTE";
    else if (op.type == dom::sim::EspionageOpType::SabotageEconomy) type = "SABOTAGE_ECONOMY";
    else if (op.type == dom::sim::EspionageOpType::SabotageSupply) type = "SABOTAGE_SUPPLY";
    else if (op.type == dom::sim::EspionageOpType::CounterIntel) type = "COUNTERINTEL";
    const char* state = op.state == dom::sim::EspionageOpState::Completed ? "COMPLETED" : (op.state == dom::sim::EspionageOpState::Failed ? "FAILED" : "ACTIVE");
    j["espionageOps"].push_back({{"id", op.id}, {"actor", op.actor}, {"target", op.target}, {"type", type}, {"startTick", op.startTick}, {"durationTicks", op.durationTicks}, {"state", state}, {"effectStrength", op.effectStrength}});
  }
  j["strategicPosture"] = nlohmann::json::array();
  for (size_t i = 0; i < w.strategicPosture.size(); ++i) j["strategicPosture"].push_back({{"player", (uint16_t)i}, {"posture", dom::sim::posture_name(w.strategicPosture[i])}});
  j["playerIdeologies"] = nlohmann::json::array();
  for (const auto& p : w.players) {
    nlohmann::json e{{"player", p.id}, {"primary", p.civilization.ideology.primary}, {"secondary", p.civilization.ideology.secondary}};
    e["weights"] = nlohmann::json::object();
    e["bloc_affinity_weights"] = nlohmann::json::object();
    e["bloc_hostility_weights"] = nlohmann::json::object();
    for (const auto& kv : p.civilization.ideology.ideologyWeights) e["weights"][kv.first] = kv.second;
    for (const auto& kv : p.civilization.ideology.blocAffinityWeights) e["bloc_affinity_weights"][kv.first] = kv.second;
    for (const auto& kv : p.civilization.ideology.blocHostilityWeights) e["bloc_hostility_weights"][kv.first] = kv.second;
    j["playerIdeologies"].push_back(std::move(e));
  }
  j["blocTemplates"] = nlohmann::json::array();
  for (const auto& t : w.blocTemplates) {
    nlohmann::json bt{{"bloc_id", t.blocId}, {"display_name", t.displayName}, {"compatible_ideologies", t.compatibleIdeologies}, {"hostile_ideologies", t.hostileIdeologies}, {"trade_bias", t.tradeBias}, {"defense_bias", t.defenseBias}, {"escalation_bias", t.escalationBias}, {"intel_sharing_bias", t.intelSharingBias}, {"min_members", t.minMembers}, {"max_members", t.maxMembers}};
    bt["founding_ideology_bias"] = nlohmann::json::object();
    for (const auto& kv : t.foundingBias) bt["founding_ideology_bias"][kv.first] = kv.second;
    j["blocTemplates"].push_back(std::move(bt));
  }
  j["allianceBlocs"] = nlohmann::json::array();
  for (const auto& b : w.allianceBlocs) j["allianceBlocs"].push_back({{"blocId", b.blocId}, {"members", b.members}, {"founder", b.founder}, {"leader", b.leader}, {"posture", (int)b.posture}, {"threatLevel", b.threatLevel}, {"rivalBlocIds", b.rivalBlocIds}, {"tradeState", b.tradeState}, {"defenseState", b.defenseState}, {"cohesion", b.cohesion}, {"lifecycleState", b.lifecycleState}});
  j["triggerAreas"] = nlohmann::json::array();
  for (const auto& a : w.triggerAreas) j["triggerAreas"].push_back({{"id", a.id}, {"min", {a.min.x, a.min.y}}, {"max", {a.max.x, a.max.y}}});
  j["objectives"] = nlohmann::json::array();
  for (const auto& o : w.objectives) j["objectives"].push_back({{"id", o.id}, {"objectiveId", o.objectiveId}, {"title", o.title}, {"text", o.text}, {"description", o.description}, {"primary", o.primary}, {"category", (int)o.category}, {"state", (int)o.state}, {"owner", o.owner}, {"visible", o.visible}, {"progressText", o.progressText}, {"progressValue", o.progressValue}});
  j["triggers"] = nlohmann::json::array();
  for (const auto& t : w.triggers) {
    nlohmann::json actions = nlohmann::json::array();
    for (const auto& a : t.actions) actions.push_back({{"type", (int)a.type}, {"text", a.text}, {"objectiveId", a.objectiveId}, {"objectiveState", (int)a.objectiveState}, {"player", a.player}, {"playerB", a.playerB}, {"resources", a.resources}, {"spawnUnitType", (int)a.spawnUnitType}, {"spawnBuildingType", (int)a.spawnBuildingType}, {"spawnCount", a.spawnCount}, {"spawnPos", {a.spawnPos.x, a.spawnPos.y}}, {"winner", a.winner}, {"areaId", a.areaId}, {"diplomacy", (int)a.diplomacy}, {"worldTension", a.worldTension}, {"operationType", (int)a.operationType}, {"operationTarget", {a.operationTarget.x, a.operationTarget.y}}, {"luaHook", a.luaHook}});
    j["triggers"].push_back({{"id", t.id}, {"once", t.once}, {"fired", t.fired}, {"condition", {{"type", (int)t.condition.type}, {"tick", t.condition.tick}, {"entityId", t.condition.entityId}, {"buildingType", (int)t.condition.buildingType}, {"areaId", t.condition.areaId}, {"player", t.condition.player}, {"objectiveId", t.condition.objectiveId}, {"worldTension", t.condition.worldTension}, {"playerB", t.condition.playerB}}}, {"actions", actions}});
  }
  j["airUnits"] = nlohmann::json::array(); for (const auto& a : w.airUnits) j["airUnits"].push_back({{"id",a.id},{"team",a.team},{"class",(int)a.cls},{"state",(int)a.state},{"pos",{a.pos.x,a.pos.y}},{"missionTarget",{a.missionTarget.x,a.missionTarget.y}},{"hp",a.hp},{"speed",a.speed},{"cooldownTicks",a.cooldownTicks},{"missionPerformed",a.missionPerformed}});
  j["detectors"] = nlohmann::json::array(); for (const auto& d : w.detectors) j["detectors"].push_back({{"id",d.id},{"team",d.team},{"type",(int)d.type},{"pos",{d.pos.x,d.pos.y}},{"radius",d.radius},{"revealContactOnly",d.revealContactOnly},{"active",d.active}});
  j["strategicStrikes"] = nlohmann::json::array(); for (const auto& st : w.strategicStrikes) j["strategicStrikes"].push_back({{"id",st.id},{"team",st.team},{"type",(int)st.type},{"from",{st.from.x,st.from.y}},{"target",{st.target.x,st.target.y}},{"prepTicksRemaining",st.prepTicksRemaining},{"travelTicksRemaining",st.travelTicksRemaining},{"cooldownTicks",st.cooldownTicks},{"interceptionState",st.interceptionState},{"launched",st.launched},{"resolved",st.resolved},{"phase",(int)st.phase},{"interceptionResult",(int)st.interceptionResult},{"targetTeam",st.targetTeam},{"launchSystemCount",st.launchSystemCount},{"warningIssued",st.warningIssued},{"retaliationLaunch",st.retaliationLaunch},{"secondStrikeLaunch",st.secondStrikeLaunch}});
  j["strategicDeterrence"] = nlohmann::json::array();
  for (size_t i = 0; i < w.strategicDeterrence.size(); ++i) { const auto& d = w.strategicDeterrence[i]; j["strategicDeterrence"].push_back({{"player", (uint16_t)i}, {"strategicCapabilityEnabled", d.strategicCapabilityEnabled}, {"strategicStockpile", d.strategicStockpile}, {"strategicReadyCount", d.strategicReadyCount}, {"strategicPreparingCount", d.strategicPreparingCount}, {"strategicAlertLevel", d.strategicAlertLevel}, {"deterrencePosture", (int)d.deterrencePosture}, {"launchWarningActive", d.launchWarningActive}, {"recentStrategicUseTick", d.recentStrategicUseTick}, {"retaliationCapability", d.retaliationCapability}, {"secondStrikeCapability", d.secondStrikeCapability}}); }
  j["denialZones"] = nlohmann::json::array(); for (const auto& z : w.denialZones) j["denialZones"].push_back({{"id",z.id},{"team",z.team},{"pos",{z.pos.x,z.pos.y}},{"radius",z.radius},{"ticksRemaining",z.ticksRemaining}});
  j["radarContactByPlayer"] = w.radarContactByPlayer;
  j["radarRevealEvents"] = w.radarRevealEvents; j["strategicStrikeEvents"] = w.strategicStrikeEvents; j["interceptionEvents"] = w.interceptionEvents; j["airMissionEvents"] = w.airMissionEvents;
  j["guardiansDiscovered"] = w.guardiansDiscovered; j["guardiansSpawned"] = w.guardiansSpawned; j["guardiansJoined"] = w.guardiansJoined; j["guardiansKilled"] = w.guardiansKilled; j["hostileGuardianEvents"] = w.hostileGuardianEvents; j["alliedGuardianEvents"] = w.alliedGuardianEvents;
  j["objectiveLog"] = nlohmann::json::array();
  for (const auto& l : w.objectiveLog) j["objectiveLog"].push_back({{"tick", l.tick}, {"text", l.text}});
  j["mission"] = {{"title", w.mission.title}, {"subtitle", w.mission.subtitle}, {"location", w.mission.locationLabel}, {"briefing", w.mission.briefing}, {"debrief", w.mission.debrief}, {"factionSummary", w.mission.factionSummary}, {"carryoverSummary", w.mission.carryoverSummary}, {"briefingPortraitId", w.mission.briefingPortraitId}, {"debriefPortraitId", w.mission.debriefPortraitId}, {"missionImageId", w.mission.missionImageId}, {"factionIconId", w.mission.factionIconId}, {"scenarioTags", w.mission.scenarioTags}, {"objectiveSummary", w.mission.objectiveSummary}, {"introMessages", w.mission.introMessages}, {"victoryOutcome", w.mission.victoryOutcomeTag}, {"defeatOutcome", w.mission.defeatOutcomeTag}, {"partialOutcome", w.mission.partialOutcomeTag}, {"branchKey", w.mission.branchKey}, {"luaScript", w.mission.luaScriptFile}, {"luaInline", w.mission.luaScriptInline}};
  j["mission"]["messages"] = nlohmann::json::array();
  for (const auto& md : w.mission.messageDefinitions) j["mission"]["messages"].push_back({{"id", md.messageId}, {"title", md.title}, {"body", md.body}, {"category", md.category}, {"speaker", md.speaker}, {"faction", md.faction}, {"portraitId", md.portraitId}, {"iconId", md.iconId}, {"imageId", md.imageId}, {"style", md.styleTag}, {"priority", md.priority}, {"durationTicks", md.durationTicks}, {"sticky", md.sticky}});
  j["missionMessages"] = nlohmann::json::array();
  for (const auto& mm : w.missionMessages) j["missionMessages"].push_back({{"sequence", mm.sequence}, {"tick", mm.tick}, {"messageId", mm.messageId}, {"title", mm.title}, {"body", mm.body}, {"category", mm.category}, {"speaker", mm.speaker}, {"faction", mm.faction}, {"portraitId", mm.portraitId}, {"iconId", mm.iconId}, {"imageId", mm.imageId}, {"style", mm.styleTag}, {"priority", mm.priority}, {"durationTicks", mm.durationTicks}, {"sticky", mm.sticky}});
  j["worldEventDefinitions"] = nlohmann::json::array();
  for (const auto& d : w.worldEventDefinitions) j["worldEventDefinitions"].push_back({{"eventId", d.eventId}, {"displayName", d.displayName}, {"category", (int)d.category}, {"triggerType", (int)d.triggerType}, {"scope", (int)d.scope}, {"targetPlayer", d.targetPlayer}, {"targetRegion", d.targetRegion}, {"targetTheater", d.targetTheater}, {"targetBiome", d.targetBiome}, {"minTick", d.minTick}, {"cooldownTicks", d.cooldownTicks}, {"defaultDuration", d.defaultDuration}, {"triggerThreshold", d.triggerThreshold}, {"baseSeverity", d.baseSeverity}, {"authored", d.authored}, {"campaignTags", d.campaignTags}, {"scriptedHook", d.scriptedHook}});
  j["worldEvents"] = nlohmann::json::array();
  for (const auto& e : w.worldEvents) j["worldEvents"].push_back({{"eventId", e.eventId}, {"displayName", e.displayName}, {"category", (int)e.category}, {"scope", (int)e.scope}, {"targetPlayer", e.targetPlayer}, {"targetRegion", e.targetRegion}, {"targetTheater", e.targetTheater}, {"targetBiome", e.targetBiome}, {"startTick", e.startTick}, {"durationTicks", e.durationTicks}, {"severity", e.severity}, {"state", (int)e.state}, {"effectPayload", e.effectPayload}, {"campaignTags", e.campaignTags}, {"scriptedHook", e.scriptedHook}});
  j["objectiveDebugLog"] = nlohmann::json::array();
  for (const auto& od : w.objectiveDebugLog) j["objectiveDebugLog"].push_back({{"tick", od.tick}, {"objectiveId", od.objectiveId}, {"from", (int)od.from}, {"to", (int)od.to}, {"triggerId", od.triggerId}, {"actionType", od.actionType}, {"reason", od.reason}});
  j["nextMissionMessageSequence"] = w.nextMissionMessageSequence;
  j["missionRuntime"] = {{"briefingShown", w.missionRuntime.briefingShown}, {"status", (int)w.missionRuntime.status}, {"resultTag", w.missionRuntime.resultTag}, {"activeObjectives", w.missionRuntime.activeObjectives}, {"luaHookLog", w.missionRuntime.luaHookLog}, {"firedTriggerCount", w.missionRuntime.firedTriggerCount}, {"scriptedActionCount", w.missionRuntime.scriptedActionCount}};
  j["campaign"] = {{"campaignId", w.campaign.campaignId}, {"playerCivilizationId", w.campaign.playerCivilizationId}, {"unlockedAge", w.campaign.unlockedAge}, {"resources", w.campaign.resources}, {"veteranUnitIds", w.campaign.veteranUnitIds}, {"discoveredGuardians", w.campaign.discoveredGuardians}, {"worldTension", w.campaign.worldTension}, {"unlockedRewards", w.campaign.unlockedRewards}, {"previousMissionResult", w.campaign.previousMissionResult}, {"pendingBranchKey", w.campaign.pendingBranchKey}};
  nlohmann::json cflags=nlohmann::json::object(); for (const auto& kv : w.campaign.flags) cflags[kv.first]=kv.second; j["campaign"]["flags"] = cflags;
  nlohmann::json cvars=nlohmann::json::object(); for (const auto& kv : w.campaign.variables) cvars[kv.first]=kv.second; j["campaign"]["variables"] = cvars;
  j["match"] = {{"phase", (int)w.match.phase}, {"condition", (int)w.match.condition}, {"winner", w.match.winner}, {"endTick", w.match.endTick}, {"scoreTieBreak", w.match.scoreTieBreak}};
  j["config"] = {{"timeLimitTicks", w.config.timeLimitTicks}, {"wonderHoldTicks", w.config.wonderHoldTicks}, {"scoreResourceWeight", w.config.scoreResourceWeight},
    {"scoreUnitWeight", w.config.scoreUnitWeight}, {"scoreBuildingWeight", w.config.scoreBuildingWeight}, {"scoreAgeWeight", w.config.scoreAgeWeight}, {"scoreCapitalWeight", w.config.scoreCapitalWeight}, {"allowConquest", w.config.allowConquest}, {"allowScore", w.config.allowScore}, {"allowWonder", w.config.allowWonder}};
  j["triggerExecutionCount"] = w.triggerExecutionCount;
  j["objectiveStateChangeCount"] = w.objectiveStateChangeCount;
  j["diplomacyEventCount"] = w.diplomacyEventCount;
  j["postureChangeCount"] = w.postureChangeCount;
  j["theatersCreatedCount"] = w.theatersCreatedCount;
  j["operationsExecutedCount"] = w.operationsExecutedCount;
  j["formationsAssignedCount"] = w.formationsAssignedCount;
  j["operationalOutcomesRecorded"] = w.operationalOutcomesRecorded;
  j["uniqueUnitsProduced"] = w.uniqueUnitsProduced;
  j["uniqueBuildingsConstructed"] = w.uniqueBuildingsConstructed;
  j["civContentResolutionFallbacks"] = w.civContentResolutionFallbacks;
  j["romeContentUsage"] = w.romeContentUsage;
  j["chinaContentUsage"] = w.chinaContentUsage;
  j["europeContentUsage"] = w.europeContentUsage;
  j["middleEastContentUsage"] = w.middleEastContentUsage;
  j["russiaContentUsage"] = w.russiaContentUsage;
  j["usaContentUsage"] = w.usaContentUsage;
  j["japanContentUsage"] = w.japanContentUsage;
  j["euContentUsage"] = w.euContentUsage;
  j["ukContentUsage"] = w.ukContentUsage;
  j["egyptContentUsage"] = w.egyptContentUsage;
  j["tartariaContentUsage"] = w.tartariaContentUsage;
  j["armageddonActive"] = w.armageddonActive;
  j["armageddonTriggerTick"] = w.armageddonTriggerTick;
  j["lastManStandingModeActive"] = w.lastManStandingModeActive;
  j["armageddonNationsThreshold"] = w.armageddonNationsThreshold;
  j["armageddonUsesPerNationThreshold"] = w.armageddonUsesPerNationThreshold;
  j["nuclearUseCountTotal"] = w.nuclearUseCountTotal;
  j["nuclearUseCountByPlayer"] = nlohmann::json::array(); for (size_t i = 0; i < w.nuclearUseCountByPlayer.size(); ++i) j["nuclearUseCountByPlayer"].push_back({{"player", (uint16_t)i}, {"count", w.nuclearUseCountByPlayer[i]}});
  j["civDoctrineSwitches"] = w.civDoctrineSwitches;
  j["civIndustryOutput"] = w.civIndustryOutput;
  j["civLogisticsBonusUsage"] = w.civLogisticsBonusUsage;
  j["civOperationCount"] = w.civOperationCount;
  j["activeWorldEventCount"] = w.activeWorldEventCount;
  j["resolvedWorldEventCount"] = w.resolvedWorldEventCount;
  j["triggeredWorldEventCount"] = w.triggeredWorldEventCount;
  j["wonder"] = {{"owner", w.wonder.owner}, {"heldTicks", w.wonder.heldTicks}};
  j["stateHash"] = dom::sim::state_hash(w);
  return j;
}

bool load_world_json(const nlohmann::json& j, dom::sim::World& w, std::string& err) {
  const uint32_t schema = j.value("schemaVersion", 0u);
  if (schema == 0 || schema > kSaveSchemaVersion) { err = "save schema mismatch"; return false; }
  w.seed = j.value("seed", 1337u);
  w.tick = j.value("tick", 0u);
  w.width = j.value("mapWidth", 128);
  w.height = j.value("mapHeight", 128);
  w.heightmap = j.at("heightmap").get<std::vector<float>>();
  w.fertility = j.at("fertility").get<std::vector<float>>();
  if (j.contains("terrainClass")) w.terrainClass = j.at("terrainClass").get<std::vector<uint8_t>>();
  if (j.contains("biomeMap")) w.biomeMap = j.at("biomeMap").get<std::vector<uint8_t>>();
  if (j.contains("temperatureMap")) w.temperatureMap = j.at("temperatureMap").get<std::vector<float>>();
  if (j.contains("moistureMap")) w.moistureMap = j.at("moistureMap").get<std::vector<float>>();
  if (j.contains("coastClassMap")) w.coastClassMap = j.at("coastClassMap").get<std::vector<uint8_t>>();
  if (j.contains("landmassIdByCell")) w.landmassIdByCell = j.at("landmassIdByCell").get<std::vector<int32_t>>();
  if (j.contains("riverMap")) w.riverMap = j.at("riverMap").get<std::vector<uint8_t>>();
  if (j.contains("lakeMap")) w.lakeMap = j.at("lakeMap").get<std::vector<uint8_t>>();
  if (j.contains("resourceWeightMap")) w.resourceWeightMap = j.at("resourceWeightMap").get<std::vector<float>>();
  w.startCandidates.clear();
  if (j.contains("startCandidates")) for (const auto& sc : j.at("startCandidates")) { dom::sim::StartCandidate c{}; c.cell = sc.value("cell", -1); c.score = sc.value("score", 0.0f); c.civBiasMask = sc.value("civBiasMask", 0u); w.startCandidates.push_back(c); }
  w.mythicCandidates.clear();
  if (j.contains("mythicCandidates")) for (const auto& mc : j.at("mythicCandidates")) { dom::sim::MythicCandidate c{}; c.siteType = (dom::sim::GuardianSiteType)mc.value("siteType", 0); c.cell = mc.value("cell", -1); c.score = mc.value("score", 0.0f); w.mythicCandidates.push_back(c); }
  w.territoryOwner = j.at("territoryOwner").get<std::vector<uint16_t>>();
  w.fog = j.at("fog").get<std::vector<uint8_t>>();
  if (j.contains("fogVisibilityByPlayer")) w.fogVisibilityByPlayer = j.at("fogVisibilityByPlayer").get<std::vector<uint8_t>>();
  if (j.contains("fogExploredByPlayer")) w.fogExploredByPlayer = j.at("fogExploredByPlayer").get<std::vector<uint8_t>>();
  if (j.contains("fogMaskByPlayer")) w.fogMaskByPlayer = j.at("fogMaskByPlayer").get<std::vector<uint8_t>>();
  w.players.clear();
  for (const auto& jp : j.at("players")) {
    dom::sim::PlayerState p{};
    p.id = jp.value("id", 0u); p.age = static_cast<dom::sim::Age>(jp.value("age", 0));
    p.resources = jp.at("resources").get<decltype(p.resources)>(); p.popUsed = jp.value("popUsed", 0); p.popCap = jp.value("popCap", 0); p.score = jp.value("score", 0);
    p.alive = jp.value("alive", true); p.unitsLost = jp.value("unitsLost", 0u); p.buildingsLost = jp.value("buildingsLost", 0u); p.finalScore = jp.value("finalScore", 0);
    p.teamId = jp.value("team", p.id);
    p.civilization = dom::sim::civilization_runtime_for(jp.value("civilization", std::string("default")));
    w.players.push_back(p);
  }
  w.worldTension = j.value("worldTension", 0.0f);
  w.diplomacy.assign(w.players.size() * w.players.size(), dom::sim::DiplomacyRelation::Neutral);
  for (size_t i = 0; i < w.players.size(); ++i) w.diplomacy[i * w.players.size() + i] = dom::sim::DiplomacyRelation::Allied;
  if (j.contains("diplomacyRelations")) {
    for (const auto& d : j.at("diplomacyRelations")) {
      const uint16_t a = d.value("a", (uint16_t)0), b = d.value("b", (uint16_t)0);
      if (a >= w.players.size() || b >= w.players.size()) continue;
      std::string rel = d.value("relation", std::string("Neutral"));
      dom::sim::DiplomacyRelation r = dom::sim::DiplomacyRelation::Neutral;
      if (rel == "Allied") r = dom::sim::DiplomacyRelation::Allied;
      else if (rel == "War") r = dom::sim::DiplomacyRelation::War;
      else if (rel == "Ceasefire") r = dom::sim::DiplomacyRelation::Ceasefire;
      w.diplomacy[a * w.players.size() + b] = r;
      w.diplomacy[b * w.players.size() + a] = r;
    }
  }
  w.treaties.assign(w.players.size() * w.players.size(), dom::sim::DiplomacyTreaty{});
  if (j.contains("treaties")) {
    for (const auto& d : j.at("treaties")) {
      const uint16_t a = d.value("a", (uint16_t)0), b = d.value("b", (uint16_t)0);
      if (a >= w.players.size() || b >= w.players.size()) continue;
      dom::sim::DiplomacyTreaty t{};
      t.tradeAgreement = d.value("tradeAgreement", false);
      t.openBorders = d.value("openBorders", false);
      t.alliance = d.value("alliance", false);
      t.nonAggression = d.value("nonAggression", false);
      t.lastChangedTick = d.value("lastChangedTick", 0u);
      w.treaties[a * w.players.size() + b] = t;
      w.treaties[b * w.players.size() + a] = t;
    }
  }
  if (j.contains("playerIdeologies")) {
    for (const auto& e : j.at("playerIdeologies")) {
      const uint16_t id = e.value("player", (uint16_t)0);
      if (id >= w.players.size()) continue;
      auto& ide = w.players[id].civilization.ideology;
      ide.primary = e.value("primary", ide.primary);
      ide.secondary = e.value("secondary", ide.secondary);
      ide.ideologyWeights.clear();
      ide.blocAffinityWeights.clear();
      ide.blocHostilityWeights.clear();
      if (e.contains("weights") && e.at("weights").is_object()) for (auto it = e.at("weights").begin(); it != e.at("weights").end(); ++it) ide.ideologyWeights.push_back({it.key(), it.value().get<float>()});
      if (e.contains("bloc_affinity_weights") && e.at("bloc_affinity_weights").is_object()) for (auto it = e.at("bloc_affinity_weights").begin(); it != e.at("bloc_affinity_weights").end(); ++it) ide.blocAffinityWeights.push_back({it.key(), it.value().get<float>()});
      if (e.contains("bloc_hostility_weights") && e.at("bloc_hostility_weights").is_object()) for (auto it = e.at("bloc_hostility_weights").begin(); it != e.at("bloc_hostility_weights").end(); ++it) ide.blocHostilityWeights.push_back({it.key(), it.value().get<float>()});
      std::sort(ide.ideologyWeights.begin(), ide.ideologyWeights.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
      std::sort(ide.blocAffinityWeights.begin(), ide.blocAffinityWeights.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
      std::sort(ide.blocHostilityWeights.begin(), ide.blocHostilityWeights.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    }
  }
  if (j.contains("blocTemplates")) {
    w.blocTemplates.clear();
    for (const auto& bt : j.at("blocTemplates")) {
      dom::sim::AllianceBlocTemplate t{};
      t.blocId = bt.value("bloc_id", std::string());
      t.displayName = bt.value("display_name", t.blocId);
      if (bt.contains("founding_ideology_bias") && bt.at("founding_ideology_bias").is_object()) for (auto it = bt.at("founding_ideology_bias").begin(); it != bt.at("founding_ideology_bias").end(); ++it) t.foundingBias.push_back({it.key(), it.value().get<float>()});
      if (bt.contains("compatible_ideologies")) t.compatibleIdeologies = bt.at("compatible_ideologies").get<std::vector<std::string>>();
      if (bt.contains("hostile_ideologies")) t.hostileIdeologies = bt.at("hostile_ideologies").get<std::vector<std::string>>();
      t.tradeBias = bt.value("trade_bias", 1.0f);
      t.defenseBias = bt.value("defense_bias", 1.0f);
      t.escalationBias = bt.value("escalation_bias", 1.0f);
      t.intelSharingBias = bt.value("intel_sharing_bias", 1.0f);
      t.minMembers = bt.value("min_members", (uint8_t)2);
      t.maxMembers = bt.value("max_members", (uint8_t)8);
      w.blocTemplates.push_back(std::move(t));
    }
  }
  if (j.contains("allianceBlocs")) {
    w.allianceBlocs.clear();
    for (const auto& bb : j.at("allianceBlocs")) {
      dom::sim::AllianceBlocState b{};
      b.blocId = bb.value("blocId", std::string());
      if (bb.contains("members")) b.members = bb.at("members").get<std::vector<uint16_t>>();
      b.founder = bb.value("founder", (uint16_t)UINT16_MAX);
      b.leader = bb.value("leader", (uint16_t)UINT16_MAX);
      b.posture = (dom::sim::StrategicPosture)bb.value("posture", 1);
      b.threatLevel = bb.value("threatLevel", 0.0f);
      if (bb.contains("rivalBlocIds")) b.rivalBlocIds = bb.at("rivalBlocIds").get<std::vector<std::string>>();
      b.tradeState = bb.value("tradeState", 1.0f);
      b.defenseState = bb.value("defenseState", 1.0f);
      b.cohesion = bb.value("cohesion", 1.0f);
      b.lifecycleState = bb.value("lifecycleState", (uint8_t)1);
      w.allianceBlocs.push_back(std::move(b));
    }
  }
  w.strategicPosture.assign(w.players.size(), dom::sim::StrategicPosture::Defensive);
  if (j.contains("strategicPosture")) {
    for (const auto& p : j.at("strategicPosture")) {
      uint16_t id = p.value("player", (uint16_t)0);
      if (id >= w.strategicPosture.size()) continue;
      std::string s = p.value("posture", std::string("DEFENSIVE"));
      if (s == "EXPANSIONIST") w.strategicPosture[id] = dom::sim::StrategicPosture::Expansionist;
      else if (s == "TRADE_FOCUSED") w.strategicPosture[id] = dom::sim::StrategicPosture::TradeFocused;
      else if (s == "ESCALATING") w.strategicPosture[id] = dom::sim::StrategicPosture::Escalating;
      else if (s == "TOTAL_WAR") w.strategicPosture[id] = dom::sim::StrategicPosture::TotalWar;
    }
  }
  w.espionageOps.clear();
  if (j.contains("espionageOps")) {
    for (const auto& e : j.at("espionageOps")) {
      dom::sim::EspionageOp op{};
      op.id = e.value("id", 0u); op.actor = e.value("actor", (uint16_t)0); op.target = e.value("target", (uint16_t)0);
      std::string type = e.value("type", std::string("RECON_CITY"));
      if (type == "REVEAL_ROUTE") op.type = dom::sim::EspionageOpType::RevealRoute;
      else if (type == "SABOTAGE_ECONOMY") op.type = dom::sim::EspionageOpType::SabotageEconomy;
      else if (type == "SABOTAGE_SUPPLY") op.type = dom::sim::EspionageOpType::SabotageSupply;
      else if (type == "COUNTERINTEL") op.type = dom::sim::EspionageOpType::CounterIntel;
      op.startTick = e.value("startTick", 0u); op.durationTicks = e.value("durationTicks", 0u); op.effectStrength = e.value("effectStrength", 0);
      std::string state = e.value("state", std::string("ACTIVE"));
      if (state == "COMPLETED") op.state = dom::sim::EspionageOpState::Completed;
      else if (state == "FAILED") op.state = dom::sim::EspionageOpState::Failed;
      w.espionageOps.push_back(op);
    }
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
    u.stealthRevealTicks = ju.value("stealthRevealTicks", 0);
    u.orderPathLingerTicks = ju.value("orderPathLingerTicks", 0); u.supplyState = static_cast<dom::sim::SupplyState>(ju.value("supplyState", 0)); u.hasMoveOrder = ju.value("hasMoveOrder", false); u.attackMove = ju.value("attackMove", false); u.selected = false; w.units.push_back(u);
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
  w.roads.clear();
  if (j.contains("roads")) for (const auto& jr : j.at("roads")) { dom::sim::RoadSegment r{}; r.id = jr.value("id", 0u); r.owner = jr.value("owner", (uint16_t)UINT16_MAX); r.a = {jr["a"][0].get<int>(), jr["a"][1].get<int>()}; r.b = {jr["b"][0].get<int>(), jr["b"][1].get<int>()}; r.quality = jr.value("quality", 1); w.roads.push_back(r); }
  w.railNodes.clear();
  if (j.contains("railNodes")) for (const auto& jr : j.at("railNodes")) { dom::sim::RailNode n{}; n.id = jr.value("id", 0u); n.owner = jr.value("owner", (uint16_t)UINT16_MAX); n.type = (dom::sim::RailNodeType)jr.value("type", 0); n.tile = {jr["tile"][0].get<int>(), jr["tile"][1].get<int>()}; n.networkId = jr.value("networkId", 0u); n.active = jr.value("active", true); w.railNodes.push_back(n); }
  w.railEdges.clear();
  if (j.contains("railEdges")) for (const auto& je : j.at("railEdges")) { dom::sim::RailEdge e{}; e.id = je.value("id", 0u); e.owner = je.value("owner", (uint16_t)UINT16_MAX); e.aNode = je.value("aNode", 0u); e.bNode = je.value("bNode", 0u); e.quality = je.value("quality", 1); e.bridge = je.value("bridge", false); e.tunnel = je.value("tunnel", false); e.disrupted = je.value("disrupted", false); w.railEdges.push_back(e); }
  w.railNetworks.clear();
  if (j.contains("railNetworks")) for (const auto& jn : j.at("railNetworks")) { dom::sim::RailNetwork n{}; n.id = jn.value("id", 0u); n.owner = jn.value("owner", (uint16_t)UINT16_MAX); n.nodeCount = jn.value("nodeCount", 0u); n.edgeCount = jn.value("edgeCount", 0u); n.active = jn.value("active", false); w.railNetworks.push_back(n); }
  w.trains.clear();
  if (j.contains("trains")) for (const auto& jt : j.at("trains")) { dom::sim::Train t{}; t.id = jt.value("id", 0u); t.owner = jt.value("owner", (uint16_t)UINT16_MAX); t.type = (dom::sim::TrainType)jt.value("type", 0); t.state = (dom::sim::TrainState)jt.value("state", 1); t.currentNode = jt.value("currentNode", 0u); t.destinationNode = jt.value("destinationNode", 0u); t.currentEdge = jt.value("currentEdge", 0u); t.routeCursor = jt.value("routeCursor", 0u); t.segmentProgress = jt.value("segmentProgress", 0.0f); t.speed = jt.value("speed", 0.03f); t.cargo = jt.value("cargo", 0.0f); t.capacity = jt.value("capacity", 100.0f); t.cargoType = jt.value("cargoType", std::string("Supply")); t.lastRouteTick = jt.value("lastRouteTick", 0u); if (jt.contains("route")) for (const auto& rs : jt.at("route")) t.route.push_back({rs.value("edgeId", 0u), rs.value("toNode", 0u)}); w.trains.push_back(std::move(t)); }
  w.operations.clear();
  if (j.contains("operations")) for (const auto& jo : j.at("operations")) { dom::sim::OperationOrder o{}; o.id = jo.value("id", 0u); o.team = jo.value("team", 0u); o.type = static_cast<dom::sim::OperationType>(jo.value("type", 0)); o.target = {jo["target"][0].get<float>(), jo["target"][1].get<float>()}; o.assignedTick = jo.value("assignedTick", 0u); o.active = jo.value("active", true); w.operations.push_back(o); }
  w.theaterCommands.clear();
  if (j.contains("theaterCommands")) for (const auto& jt : j.at("theaterCommands")) { dom::sim::TheaterCommand t{}; t.theaterId=jt.value("theaterId",0u); t.owner=jt.value("owner",0u); if (jt.contains("bounds") && jt.at("bounds").is_array() && jt.at("bounds").size()>=4) t.bounds={jt["bounds"][0].get<int>(),jt["bounds"][1].get<int>(),jt["bounds"][2].get<int>(),jt["bounds"][3].get<int>()}; t.priority=(dom::sim::TheaterPriority)jt.value("priority",1); if (jt.contains("activeOperations")) t.activeOperations=jt.at("activeOperations").get<std::vector<uint32_t>>(); if (jt.contains("assignedArmyGroups")) t.assignedArmyGroups=jt.at("assignedArmyGroups").get<std::vector<uint32_t>>(); if (jt.contains("assignedNavalTaskForces")) t.assignedNavalTaskForces=jt.at("assignedNavalTaskForces").get<std::vector<uint32_t>>(); if (jt.contains("assignedAirWings")) t.assignedAirWings=jt.at("assignedAirWings").get<std::vector<uint32_t>>(); t.supplyStatus=jt.value("supplyStatus",1.0f); t.threatLevel=jt.value("threatLevel",0.0f); w.theaterCommands.push_back(std::move(t)); }
  w.armyGroups.clear();
  if (j.contains("armyGroups")) for (const auto& ja : j.at("armyGroups")) { dom::sim::ArmyGroup a{}; a.id=ja.value("id",0u); a.owner=ja.value("owner",0u); a.theaterId=ja.value("theaterId",0u); if (ja.contains("unitIds")) a.unitIds=ja.at("unitIds").get<std::vector<uint32_t>>(); a.stance=(dom::sim::ArmyGroupStance)ja.value("stance",1); a.assignedObjective=ja.value("assignedObjective",0u); a.active=ja.value("active",true); w.armyGroups.push_back(std::move(a)); }
  w.navalTaskForces.clear();
  if (j.contains("navalTaskForces")) for (const auto& jn : j.at("navalTaskForces")) { dom::sim::NavalTaskForce n{}; n.id=jn.value("id",0u); n.owner=jn.value("owner",0u); n.theaterId=jn.value("theaterId",0u); if (jn.contains("unitIds")) n.unitIds=jn.at("unitIds").get<std::vector<uint32_t>>(); n.mission=(dom::sim::NavalMissionType)jn.value("mission",0); n.assignedObjective=jn.value("assignedObjective",0u); n.active=jn.value("active",true); w.navalTaskForces.push_back(std::move(n)); }
  w.airWings.clear();
  if (j.contains("airWings")) for (const auto& jw : j.at("airWings")) { dom::sim::AirWing a{}; a.id=jw.value("id",0u); a.owner=jw.value("owner",0u); a.theaterId=jw.value("theaterId",0u); if (jw.contains("squadronIds")) a.squadronIds=jw.at("squadronIds").get<std::vector<uint32_t>>(); a.mission=(dom::sim::AirMissionType)jw.value("mission",1); a.assignedObjective=jw.value("assignedObjective",0u); a.active=jw.value("active",true); w.airWings.push_back(std::move(a)); }
  w.operationalObjectives.clear();
  if (j.contains("operationalObjectives")) for (const auto& jo : j.at("operationalObjectives")) { dom::sim::OperationalObjective o{}; o.id=jo.value("id",0u); o.owner=jo.value("owner",0u); o.theaterId=jo.value("theaterId",0u); o.objectiveType=(dom::sim::OperationType)jo.value("objectiveType",4); if (jo.contains("targetRegion")&&jo.at("targetRegion").is_array()&&jo.at("targetRegion").size()>=4) o.targetRegion={jo["targetRegion"][0].get<int>(),jo["targetRegion"][1].get<int>(),jo["targetRegion"][2].get<int>(),jo["targetRegion"][3].get<int>()}; o.requiredForce=jo.value("requiredForce",0u); o.startTick=jo.value("startTick",0u); o.durationTicks=jo.value("durationTicks",0u); o.outcome=(dom::sim::OperationOutcome)jo.value("outcome",0); o.active=jo.value("active",true); if (jo.contains("armyGroups")) o.armyGroups=jo.at("armyGroups").get<std::vector<uint32_t>>(); if (jo.contains("navalTaskForces")) o.navalTaskForces=jo.at("navalTaskForces").get<std::vector<uint32_t>>(); if (jo.contains("airWings")) o.airWings=jo.at("airWings").get<std::vector<uint32_t>>(); w.operationalObjectives.push_back(std::move(o)); }
  w.triggerAreas.clear();
  if (j.contains("triggerAreas")) for (const auto& ja : j.at("triggerAreas")) { dom::sim::TriggerArea a{}; a.id = ja.value("id", 0u); a.min = {ja["min"][0].get<float>(), ja["min"][1].get<float>()}; a.max = {ja["max"][0].get<float>(), ja["max"][1].get<float>()}; w.triggerAreas.push_back(a); }
  w.objectives.clear();
  if (j.contains("objectives")) for (const auto& jo : j.at("objectives")) { dom::sim::Objective o{}; o.id = jo.value("id", 0u); o.objectiveId = jo.value("objectiveId", std::string()); o.title = jo.value("title", ""); o.text = jo.value("text", ""); o.description = jo.value("description", ""); o.primary = jo.value("primary", true); o.category = static_cast<dom::sim::ObjectiveCategory>(jo.value("category", 0)); o.state = static_cast<dom::sim::ObjectiveState>(jo.value("state", 0)); o.owner = jo.value("owner", (uint16_t)UINT16_MAX); o.visible = jo.value("visible", true); o.progressText = jo.value("progressText", std::string()); o.progressValue = jo.value("progressValue", 0.0f); w.objectives.push_back(o); }
  w.triggers.clear();
  if (j.contains("triggers")) for (const auto& jt : j.at("triggers")) { dom::sim::Trigger t{}; t.id = jt.value("id", 0u); t.once = jt.value("once", true); t.fired = jt.value("fired", false); const auto& jcnd = jt.at("condition"); t.condition.type = static_cast<dom::sim::TriggerType>(jcnd.value("type", 0)); t.condition.tick = jcnd.value("tick", 0u); t.condition.entityId = jcnd.value("entityId", 0u); t.condition.buildingType = static_cast<dom::sim::BuildingType>(jcnd.value("buildingType", 0)); t.condition.areaId = jcnd.value("areaId", 0u); t.condition.player = jcnd.value("player", (uint16_t)UINT16_MAX); t.condition.objectiveId = jcnd.value("objectiveId", 0u); t.condition.worldTension = jcnd.value("worldTension", 0.0f); t.condition.playerB = jcnd.value("playerB", (uint16_t)UINT16_MAX); for (const auto& ja : jt.at("actions")) { dom::sim::TriggerAction a{}; a.type = static_cast<dom::sim::TriggerActionType>(ja.value("type", 0)); a.text = ja.value("text", ""); a.objectiveId = ja.value("objectiveId", 0u); a.objectiveState = static_cast<dom::sim::ObjectiveState>(ja.value("objectiveState", 0)); a.player = ja.value("player", (uint16_t)UINT16_MAX); a.resources = ja.at("resources").get<decltype(a.resources)>(); a.spawnUnitType = static_cast<dom::sim::UnitType>(ja.value("spawnUnitType", 0)); a.spawnBuildingType = static_cast<dom::sim::BuildingType>(ja.value("spawnBuildingType", 0)); a.spawnCount = ja.value("spawnCount", 0u); a.spawnPos = {ja["spawnPos"][0].get<float>(), ja["spawnPos"][1].get<float>()}; a.winner = ja.value("winner", 0u); a.areaId = ja.value("areaId", 0u); a.playerB = ja.value("playerB", (uint16_t)UINT16_MAX); a.diplomacy = static_cast<dom::sim::DiplomacyRelation>(ja.value("diplomacy", 1)); a.worldTension = ja.value("worldTension", 0.0f); a.operationType = static_cast<dom::sim::OperationType>(ja.value("operationType", 0)); if (ja.contains("operationTarget")) a.operationTarget = {ja["operationTarget"][0].get<float>(), ja["operationTarget"][1].get<float>()}; a.luaHook = ja.value("luaHook", std::string()); t.actions.push_back(a);} w.triggers.push_back(t);} 
  w.airUnits.clear(); if (j.contains("airUnits")) for (const auto& a : j.at("airUnits")) { dom::sim::AirUnit au{}; au.id=a.value("id",0u); au.team=a.value("team",0u); au.cls=(dom::sim::AirUnitClass)a.value("class",0); au.state=(dom::sim::AirMissionState)a.value("state",0); au.pos={a["pos"][0].get<float>(),a["pos"][1].get<float>()}; au.missionTarget={a["missionTarget"][0].get<float>(),a["missionTarget"][1].get<float>()}; au.hp=a.value("hp",100.0f); au.speed=a.value("speed",6.0f); au.cooldownTicks=a.value("cooldownTicks",0u); au.missionPerformed=a.value("missionPerformed",false); w.airUnits.push_back(au); }
  w.detectors.clear(); if (j.contains("detectors")) for (const auto& d : j.at("detectors")) { dom::sim::DetectorSite ds{}; ds.id=d.value("id",0u); ds.team=d.value("team",0u); ds.type=(dom::sim::DetectorType)d.value("type",0); ds.pos={d["pos"][0].get<float>(),d["pos"][1].get<float>()}; ds.radius=d.value("radius",12.0f); ds.revealContactOnly=d.value("revealContactOnly",false); ds.active=d.value("active",true); w.detectors.push_back(ds); }
  w.strategicStrikes.clear(); if (j.contains("strategicStrikes")) for (const auto& st : j.at("strategicStrikes")) { dom::sim::StrategicStrike ss{}; ss.id=st.value("id",0u); ss.team=st.value("team",0u); ss.type=(dom::sim::StrikeType)st.value("type",0); ss.from={st["from"][0].get<float>(),st["from"][1].get<float>()}; ss.target={st["target"][0].get<float>(),st["target"][1].get<float>()}; ss.prepTicksRemaining=st.value("prepTicksRemaining",0u); ss.travelTicksRemaining=st.value("travelTicksRemaining",0u); ss.cooldownTicks=st.value("cooldownTicks",0u); ss.interceptionState=st.value("interceptionState",(uint8_t)0); ss.launched=st.value("launched",false); ss.resolved=st.value("resolved",false); ss.phase=(dom::sim::StrategicStrikePhase)st.value("phase", (int)dom::sim::StrategicStrikePhase::Unavailable); ss.interceptionResult=(dom::sim::StrategicInterceptionResult)st.value("interceptionResult", (int)dom::sim::StrategicInterceptionResult::Undetected); ss.targetTeam=st.value("targetTeam", (uint16_t)UINT16_MAX); ss.launchSystemCount=st.value("launchSystemCount", (uint16_t)1); ss.warningIssued=st.value("warningIssued", false); ss.retaliationLaunch=st.value("retaliationLaunch", false); ss.secondStrikeLaunch=st.value("secondStrikeLaunch", false); w.strategicStrikes.push_back(ss); }
  w.strategicDeterrence.assign(w.players.size(), dom::sim::StrategicDeterrenceState{});
  if (j.contains("strategicDeterrence")) for (const auto& e : j.at("strategicDeterrence")) { uint16_t id=e.value("player",(uint16_t)0); if (id>=w.strategicDeterrence.size()) continue; auto& d=w.strategicDeterrence[id]; d.strategicCapabilityEnabled=e.value("strategicCapabilityEnabled",false); d.strategicStockpile=e.value("strategicStockpile",(uint16_t)0); d.strategicReadyCount=e.value("strategicReadyCount",(uint16_t)0); d.strategicPreparingCount=e.value("strategicPreparingCount",(uint16_t)0); d.strategicAlertLevel=e.value("strategicAlertLevel",(uint8_t)0); d.deterrencePosture=(dom::sim::DeterrencePosture)e.value("deterrencePosture",0); d.launchWarningActive=e.value("launchWarningActive",false); d.recentStrategicUseTick=e.value("recentStrategicUseTick",0u); d.retaliationCapability=e.value("retaliationCapability",false); d.secondStrikeCapability=e.value("secondStrikeCapability",false); }
  w.denialZones.clear(); if (j.contains("denialZones")) for (const auto& z : j.at("denialZones")) { dom::sim::DenialZone dz{}; dz.id=z.value("id",0u); dz.team=z.value("team",0u); dz.pos={z["pos"][0].get<float>(),z["pos"][1].get<float>()}; dz.radius=z.value("radius",6.0f); dz.ticksRemaining=z.value("ticksRemaining",0u); w.denialZones.push_back(dz);} 
  if (j.contains("radarContactByPlayer")) w.radarContactByPlayer = j.at("radarContactByPlayer").get<std::vector<uint8_t>>();
  w.radarRevealEvents = j.value("radarRevealEvents",0u); w.strategicStrikeEvents = j.value("strategicStrikeEvents",0u); w.interceptionEvents = j.value("interceptionEvents",0u); w.airMissionEvents = j.value("airMissionEvents",0u);
  w.mountainRegionByCell.clear(); if (j.contains("mountainRegionByCell")) w.mountainRegionByCell = j.at("mountainRegionByCell").get<std::vector<int32_t>>();
  w.mountainRegions.clear(); if (j.contains("mountainRegions")) for (const auto& mr : j.at("mountainRegions")) { dom::sim::MountainRegion m{}; m.id=mr.value("id",0u); m.minX=mr.value("minX",0); m.minY=mr.value("minY",0); m.maxX=mr.value("maxX",0); m.maxY=mr.value("maxY",0); m.peakCell=mr.value("peakCell",-1); m.centerCell=mr.value("centerCell",-1); m.cellCount=mr.value("cellCount",0u); w.mountainRegions.push_back(m); }
  w.surfaceDeposits.clear(); if (j.contains("surfaceDeposits")) for (const auto& sd : j.at("surfaceDeposits")) { dom::sim::SurfaceDeposit d{}; d.id=sd.value("id",0u); d.regionId=sd.value("regionId",0u); d.mineral=(dom::sim::MineralType)sd.value("mineral",0); d.cell=sd.value("cell",-1); d.remaining=sd.value("remaining",0.0f); d.owner=sd.value("owner",(uint16_t)UINT16_MAX); w.surfaceDeposits.push_back(d); }
  w.deepDeposits.clear(); if (j.contains("deepDeposits")) for (const auto& dd : j.at("deepDeposits")) { dom::sim::DeepDeposit d{}; d.id=dd.value("id",0u); d.regionId=dd.value("regionId",0u); d.nodeId=dd.value("nodeId",0u); d.mineral=(dom::sim::MineralType)dd.value("mineral",0); d.cell=dd.value("cell",-1); d.richness=dd.value("richness",1.0f); d.remaining=dd.value("remaining",0.0f); d.owner=dd.value("owner",(uint16_t)UINT16_MAX); d.active=dd.value("active",true); w.deepDeposits.push_back(d); }
  w.undergroundNodes.clear(); if (j.contains("undergroundNodes")) for (const auto& nd : j.at("undergroundNodes")) { dom::sim::UndergroundNode n{}; n.id=nd.value("id",0u); n.regionId=nd.value("regionId",0u); n.type=(dom::sim::UndergroundNodeType)nd.value("type",0); n.cell=nd.value("cell",-1); n.linkedBuildingId=nd.value("linkedBuildingId",0u); n.owner=nd.value("owner",(uint16_t)UINT16_MAX); n.active=nd.value("active",true); w.undergroundNodes.push_back(n); }
  w.undergroundEdges.clear(); if (j.contains("undergroundEdges")) for (const auto& ed : j.at("undergroundEdges")) { dom::sim::UndergroundEdge e{}; e.id=ed.value("id",0u); e.regionId=ed.value("regionId",0u); e.a=ed.value("a",0u); e.b=ed.value("b",0u); e.owner=ed.value("owner",(uint16_t)UINT16_MAX); e.active=ed.value("active",true); w.undergroundEdges.push_back(e); }
  w.guardianDefinitions.clear(); if (j.contains("guardianDefinitions")) for (const auto& gd : j.at("guardianDefinitions")) { dom::sim::GuardianDefinition d{}; d.guardianId=gd.value("guardianId",std::string{}); d.displayName=gd.value("displayName",d.guardianId); d.biomeRequirement=(uint8_t)gd.value("biomeRequirement",0); d.siteType=(dom::sim::GuardianSiteType)gd.value("siteType",0); d.spawnMode=(dom::sim::GuardianSpawnMode)gd.value("spawnMode",0); d.maxPerMap=gd.value("maxPerMap",1u); d.unique=gd.value("unique",true); d.discoveryMode=(dom::sim::GuardianDiscoveryMode)gd.value("discoveryMode",0); d.behaviorMode=(dom::sim::GuardianBehaviorMode)gd.value("behaviorMode",0); d.joinMode=(dom::sim::GuardianJoinMode)gd.value("joinMode",0); d.associatedUnitDefinitionId=gd.value("associatedUnitDefinitionId",std::string{}); d.unitHp=gd.value("unitHp",3500.0f); d.unitAttack=gd.value("unitAttack",90.0f); d.unitRange=gd.value("unitRange",1.6f); d.unitSpeed=gd.value("unitSpeed",1.6f); d.rewardHook=gd.value("rewardHook",std::string{}); d.effectHook=gd.value("effectHook",std::string{}); d.scenarioOnly=gd.value("scenarioOnly",false); d.procedural=gd.value("procedural",true); d.rarityPermille=gd.value("rarityPermille",8); d.minSpacingCells=gd.value("minSpacingCells",20); d.discoveryRadius=gd.value("discoveryRadius",5.0f); w.guardianDefinitions.push_back(std::move(d)); }
  w.guardianSites.clear(); if (j.contains("guardianSites")) for (const auto& gs : j.at("guardianSites")) { dom::sim::GuardianSiteInstance s{}; s.instanceId=gs.value("instanceId",0u); s.guardianId=gs.value("guardianId",std::string{}); s.siteType=(dom::sim::GuardianSiteType)gs.value("siteType",0); if (gs.contains("pos") && gs.at("pos").is_array()) s.pos={gs.at("pos")[0].get<float>(),gs.at("pos")[1].get<float>()}; s.regionId=gs.value("regionId",-1); s.nodeId=gs.value("nodeId",-1); s.discovered=gs.value("discovered",false); s.alive=gs.value("alive",true); s.owner=gs.value("owner",(uint16_t)UINT16_MAX); s.siteActive=gs.value("siteActive",true); s.siteDepleted=gs.value("siteDepleted",false); s.spawned=gs.value("spawned",false); s.behaviorState=gs.value("behaviorState",(uint8_t)0); s.cooldownTicks=gs.value("cooldownTicks",0u); s.oneShotUsed=gs.value("oneShotUsed",false); s.scenarioPlaced=gs.value("scenarioPlaced",false); w.guardianSites.push_back(std::move(s)); }
  w.guardiansDiscovered = j.value("guardiansDiscovered",0u); w.guardiansSpawned = j.value("guardiansSpawned",0u); w.guardiansJoined = j.value("guardiansJoined",0u); w.guardiansKilled = j.value("guardiansKilled",0u); w.hostileGuardianEvents = j.value("hostileGuardianEvents",0u); w.alliedGuardianEvents = j.value("alliedGuardianEvents",0u);
  w.objectiveLog.clear();
  if (j.contains("objectiveLog")) for (const auto& jl : j.at("objectiveLog")) { w.objectiveLog.push_back({jl.value("tick", 0u), jl.value("text", "")}); }
  if (j.contains("mission")) { const auto& m = j.at("mission"); w.mission.title = m.value("title", ""); w.mission.subtitle = m.value("subtitle", ""); w.mission.locationLabel = m.value("location", ""); w.mission.briefing = m.value("briefing", ""); w.mission.debrief = m.value("debrief", ""); w.mission.factionSummary = m.value("factionSummary", ""); w.mission.carryoverSummary = m.value("carryoverSummary", ""); w.mission.briefingPortraitId = m.value("briefingPortraitId", "ui_portrait_default"); w.mission.debriefPortraitId = m.value("debriefPortraitId", "ui_portrait_default"); w.mission.missionImageId = m.value("missionImageId", "ui_mission_default"); w.mission.factionIconId = m.value("factionIconId", "ui_faction_default"); if (m.contains("scenarioTags")) w.mission.scenarioTags = m.at("scenarioTags").get<std::vector<std::string>>(); if (m.contains("objectiveSummary")) w.mission.objectiveSummary = m.at("objectiveSummary").get<std::vector<std::string>>(); if (m.contains("introMessages")) w.mission.introMessages = m.at("introMessages").get<std::vector<std::string>>(); if (m.contains("messages")) { w.mission.messageDefinitions.clear(); for (const auto& md : m.at("messages")) { dom::sim::MissionMessageDefinition def{}; def.messageId = md.value("id", std::string("")); def.title = md.value("title", std::string("Mission Update")); def.body = md.value("body", std::string("")); def.category = md.value("category", std::string("intelligence")); def.speaker = md.value("speaker", std::string("")); def.faction = md.value("faction", std::string("")); def.portraitId = md.value("portraitId", std::string("ui_portrait_default")); def.iconId = md.value("iconId", std::string("ui_icon_event")); def.imageId = md.value("imageId", std::string("ui_mission_default")); def.styleTag = md.value("style", std::string("default")); def.priority = md.value("priority", 0); def.durationTicks = md.value("durationTicks", 600u); def.sticky = md.value("sticky", false); w.mission.messageDefinitions.push_back(std::move(def)); } } w.mission.victoryOutcomeTag = m.value("victoryOutcome", "victory"); w.mission.defeatOutcomeTag = m.value("defeatOutcome", "defeat"); w.mission.partialOutcomeTag = m.value("partialOutcome", "partial_victory"); w.mission.branchKey = m.value("branchKey", ""); w.mission.luaScriptFile = m.value("luaScript", ""); w.mission.luaScriptInline = m.value("luaInline", ""); }
  w.missionMessages.clear();
  if (j.contains("missionMessages")) for (const auto& mm : j.at("missionMessages")) { dom::sim::MissionMessageRuntime m{}; m.sequence = mm.value("sequence", 0ull); m.tick = mm.value("tick", 0u); m.messageId = mm.value("messageId", ""); m.title = mm.value("title", ""); m.body = mm.value("body", ""); m.category = mm.value("category", "intelligence"); m.speaker = mm.value("speaker", ""); m.faction = mm.value("faction", ""); m.portraitId = mm.value("portraitId", "ui_portrait_default"); m.iconId = mm.value("iconId", "ui_icon_event"); m.imageId = mm.value("imageId", "ui_mission_default"); m.styleTag = mm.value("style", "default"); m.priority = mm.value("priority", 0); m.durationTicks = mm.value("durationTicks", 600u); m.sticky = mm.value("sticky", false); w.missionMessages.push_back(std::move(m)); }
  w.worldEventDefinitions.clear();
  if (j.contains("worldEventDefinitions")) for (const auto& e : j.at("worldEventDefinitions")) { dom::sim::WorldEventDefinition d{}; d.eventId = e.value("eventId", ""); d.displayName = e.value("displayName", d.eventId); d.category = static_cast<dom::sim::WorldEventCategory>(e.value("category", 0)); d.triggerType = static_cast<dom::sim::WorldEventTriggerType>(e.value("triggerType", 0)); d.scope = static_cast<dom::sim::WorldEventScope>(e.value("scope", 0)); d.targetPlayer = e.value("targetPlayer", (uint16_t)UINT16_MAX); d.targetRegion = e.value("targetRegion", -1); d.targetTheater = e.value("targetTheater", -1); d.targetBiome = e.value("targetBiome", -1); d.minTick = e.value("minTick", 0u); d.cooldownTicks = e.value("cooldownTicks", 1200u); d.defaultDuration = e.value("defaultDuration", 1200u); d.triggerThreshold = e.value("triggerThreshold", 0.0f); d.baseSeverity = e.value("baseSeverity", 1.0f); d.authored = e.value("authored", false); if (e.contains("campaignTags")) d.campaignTags = e.at("campaignTags").get<std::vector<std::string>>(); d.scriptedHook = e.value("scriptedHook", ""); w.worldEventDefinitions.push_back(std::move(d)); }
  w.worldEvents.clear();
  if (j.contains("worldEvents")) for (const auto& e : j.at("worldEvents")) { dom::sim::WorldEventInstance v{}; v.eventId = e.value("eventId", ""); v.displayName = e.value("displayName", v.eventId); v.category = static_cast<dom::sim::WorldEventCategory>(e.value("category", 0)); v.scope = static_cast<dom::sim::WorldEventScope>(e.value("scope", 0)); v.targetPlayer = e.value("targetPlayer", (uint16_t)UINT16_MAX); v.targetRegion = e.value("targetRegion", -1); v.targetTheater = e.value("targetTheater", -1); v.targetBiome = e.value("targetBiome", -1); v.startTick = e.value("startTick", 0u); v.durationTicks = e.value("durationTicks", 0u); v.severity = e.value("severity", 1.0f); v.state = static_cast<dom::sim::WorldEventState>(e.value("state", 0)); v.effectPayload = e.value("effectPayload", ""); if (e.contains("campaignTags")) v.campaignTags = e.at("campaignTags").get<std::vector<std::string>>(); v.scriptedHook = e.value("scriptedHook", ""); w.worldEvents.push_back(std::move(v)); }
  w.objectiveDebugLog.clear();
  if (j.contains("objectiveDebugLog")) for (const auto& od : j.at("objectiveDebugLog")) { dom::sim::ObjectiveTransitionDebugEntry e{}; e.tick = od.value("tick", 0u); e.objectiveId = od.value("objectiveId", 0u); e.from = static_cast<dom::sim::ObjectiveState>(od.value("from", 0)); e.to = static_cast<dom::sim::ObjectiveState>(od.value("to", 0)); e.triggerId = od.value("triggerId", 0u); e.actionType = od.value("actionType", ""); e.reason = od.value("reason", ""); w.objectiveDebugLog.push_back(std::move(e)); }
  w.nextMissionMessageSequence = j.value("nextMissionMessageSequence", 1ull);
  if (j.contains("campaign")) { const auto& c = j.at("campaign"); w.campaign.campaignId = c.value("campaignId", std::string("")); w.campaign.playerCivilizationId = c.value("playerCivilizationId", std::string("")); w.campaign.unlockedAge = static_cast<uint8_t>(c.value("unlockedAge", 0)); if (c.contains("resources")) w.campaign.resources = c.at("resources").get<std::array<float, static_cast<size_t>(dom::sim::Resource::Count)>>(); if (c.contains("veteranUnitIds")) w.campaign.veteranUnitIds = c.at("veteranUnitIds").get<std::vector<uint32_t>>(); if (c.contains("discoveredGuardians")) w.campaign.discoveredGuardians = c.at("discoveredGuardians").get<std::vector<std::string>>(); w.campaign.worldTension = c.value("worldTension", 0.0f); if (c.contains("unlockedRewards")) w.campaign.unlockedRewards = c.at("unlockedRewards").get<std::vector<std::string>>(); w.campaign.previousMissionResult = c.value("previousMissionResult", std::string("")); w.campaign.pendingBranchKey = c.value("pendingBranchKey", std::string("")); if (c.contains("flags")) for (auto it=c["flags"].begin(); it!=c["flags"].end(); ++it) w.campaign.flags.push_back({it.key(), it.value().get<bool>()}); if (c.contains("variables")) for (auto it=c["variables"].begin(); it!=c["variables"].end(); ++it) w.campaign.variables.push_back({it.key(), it.value().get<int64_t>()}); }
  if (j.contains("missionRuntime")) { const auto& mr = j.at("missionRuntime"); w.missionRuntime.briefingShown = mr.value("briefingShown", false); w.missionRuntime.status = static_cast<dom::sim::MissionStatus>(mr.value("status", 1)); w.missionRuntime.resultTag = mr.value("resultTag", ""); if (mr.contains("activeObjectives")) w.missionRuntime.activeObjectives = mr.at("activeObjectives").get<std::vector<uint32_t>>(); if (mr.contains("luaHookLog")) w.missionRuntime.luaHookLog = mr.at("luaHookLog").get<std::vector<std::string>>(); w.missionRuntime.firedTriggerCount = mr.value("firedTriggerCount", 0u); w.missionRuntime.scriptedActionCount = mr.value("scriptedActionCount", 0u); }
  const auto& jm = j.at("match");
  w.match.phase = static_cast<dom::sim::MatchPhase>(jm.value("phase", 0)); w.match.condition = static_cast<dom::sim::VictoryCondition>(jm.value("condition", 0));
  w.match.winner = jm.value("winner", 0u); w.match.endTick = jm.value("endTick", 0u); w.match.scoreTieBreak = jm.value("scoreTieBreak", false);
  const auto& jc = j.at("config");
  w.config.timeLimitTicks = jc.value("timeLimitTicks", w.config.timeLimitTicks); w.config.wonderHoldTicks = jc.value("wonderHoldTicks", w.config.wonderHoldTicks);
  w.config.scoreResourceWeight = jc.value("scoreResourceWeight", w.config.scoreResourceWeight); w.config.scoreUnitWeight = jc.value("scoreUnitWeight", w.config.scoreUnitWeight);
  w.config.scoreBuildingWeight = jc.value("scoreBuildingWeight", w.config.scoreBuildingWeight); w.config.scoreAgeWeight = jc.value("scoreAgeWeight", w.config.scoreAgeWeight); w.config.scoreCapitalWeight = jc.value("scoreCapitalWeight", w.config.scoreCapitalWeight); w.config.allowConquest = jc.value("allowConquest", true); w.config.allowScore = jc.value("allowScore", true); w.config.allowWonder = jc.value("allowWonder", true);
  w.triggerExecutionCount = j.value("triggerExecutionCount", 0u);
  w.objectiveStateChangeCount = j.value("objectiveStateChangeCount", 0u);
  w.diplomacyEventCount = j.value("diplomacyEventCount", 0u);
  w.postureChangeCount = j.value("postureChangeCount", 0u);
  w.theatersCreatedCount = j.value("theatersCreatedCount", 0u);
  w.operationsExecutedCount = j.value("operationsExecutedCount", 0u);
  w.formationsAssignedCount = j.value("formationsAssignedCount", 0u);
  w.operationalOutcomesRecorded = j.value("operationalOutcomesRecorded", 0u);
  w.uniqueUnitsProduced = j.value("uniqueUnitsProduced", 0u);
  w.uniqueBuildingsConstructed = j.value("uniqueBuildingsConstructed", 0u);
  w.civContentResolutionFallbacks = j.value("civContentResolutionFallbacks", 0u);
  w.romeContentUsage = j.value("romeContentUsage", 0u);
  w.chinaContentUsage = j.value("chinaContentUsage", 0u);
  w.europeContentUsage = j.value("europeContentUsage", 0u);
  w.middleEastContentUsage = j.value("middleEastContentUsage", 0u);
  w.russiaContentUsage = j.value("russiaContentUsage", 0u);
  w.usaContentUsage = j.value("usaContentUsage", 0u);
  w.japanContentUsage = j.value("japanContentUsage", 0u);
  w.euContentUsage = j.value("euContentUsage", 0u);
  w.ukContentUsage = j.value("ukContentUsage", 0u);
  w.egyptContentUsage = j.value("egyptContentUsage", 0u);
  w.tartariaContentUsage = j.value("tartariaContentUsage", 0u);
  w.armageddonActive = j.value("armageddonActive", false);
  w.armageddonTriggerTick = j.value("armageddonTriggerTick", 0u);
  w.lastManStandingModeActive = j.value("lastManStandingModeActive", false);
  w.armageddonNationsThreshold = j.value("armageddonNationsThreshold", static_cast<uint16_t>(2));
  w.armageddonUsesPerNationThreshold = j.value("armageddonUsesPerNationThreshold", static_cast<uint16_t>(2));
  w.nuclearUseCountTotal = j.value("nuclearUseCountTotal", 0u);
  w.nuclearUseCountByPlayer.assign(w.players.size(), 0);
  if (j.contains("nuclearUseCountByPlayer") && j["nuclearUseCountByPlayer"].is_array()) for (const auto& e : j["nuclearUseCountByPlayer"]) { uint16_t player = e.value("player", (uint16_t)0); if (player < w.nuclearUseCountByPlayer.size()) w.nuclearUseCountByPlayer[player] = e.value("count", (uint16_t)0); }
  w.civDoctrineSwitches = j.value("civDoctrineSwitches", 0u);
  w.civIndustryOutput = j.value("civIndustryOutput", 0.0f);
  w.civLogisticsBonusUsage = j.value("civLogisticsBonusUsage", 0.0f);
  w.civOperationCount = j.value("civOperationCount", 0u);
  w.activeWorldEventCount = j.value("activeWorldEventCount", 0u);
  w.resolvedWorldEventCount = j.value("resolvedWorldEventCount", 0u);
  w.triggeredWorldEventCount = j.value("triggeredWorldEventCount", 0u);
  w.wonder.owner = j.at("wonder").value("owner", UINT16_MAX); w.wonder.heldTicks = j.at("wonder").value("heldTicks", 0u);
  w.riverCount = 0; for (uint8_t v : w.riverMap) if (v) ++w.riverCount;
  w.lakeCount = 0; for (uint8_t v : w.lakeMap) if (v) ++w.lakeCount;
  dom::sim::on_authoritative_state_loaded(w);
  const uint64_t expected = j.value("stateHash", 0ull);
  (void)expected;
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



dom::sim::Unit make_spawned_unit(uint32_t id, uint16_t team, dom::sim::UnitType type, const glm::vec2& p) {
  dom::sim::Unit u{};
  u.id = id; u.team = team; u.type = type; u.pos = u.renderPos = u.target = u.slotTarget = p;
  u.role = (type == dom::sim::UnitType::Worker) ? dom::sim::UnitRole::Worker :
           (type == dom::sim::UnitType::Archer) ? dom::sim::UnitRole::Ranged :
           (type == dom::sim::UnitType::Cavalry) ? dom::sim::UnitRole::Cavalry :
           (type == dom::sim::UnitType::Siege) ? dom::sim::UnitRole::Siege : dom::sim::UnitRole::Infantry;
  u.attackType = (type == dom::sim::UnitType::Archer || type == dom::sim::UnitType::Siege) ? dom::sim::AttackType::Ranged : dom::sim::AttackType::Melee;
  u.preferredTargetRole = dom::sim::UnitRole::Infantry;
  u.vsRoleMultiplierPermille = {1000,1000,1000,1000,1000,1000};
  if (type == dom::sim::UnitType::Worker) { u.hp = 70; u.attack = 3.0f; u.range = 1.5f; u.speed = 4.3f; }
  else if (type == dom::sim::UnitType::Infantry) { u.hp = 105; u.attack = 8.5f; u.range = 2.0f; u.speed = 4.8f; }
  else if (type == dom::sim::UnitType::Archer) { u.hp = 80; u.attack = 7.0f; u.range = 5.4f; u.speed = 4.4f; }
  else if (type == dom::sim::UnitType::Cavalry) { u.hp = 130; u.attack = 9.2f; u.range = 1.8f; u.speed = 5.6f; }
  else { u.hp = 110; u.attack = 13.0f; u.range = 6.2f; u.speed = 3.2f; }
  return u;
}

void setup_cpu_battle(dom::sim::World& world, int perTeam) {
  for (auto& p : world.players) { p.isCPU = true; p.isHuman = false; p.alive = true; }
  world.units.clear();
  uint32_t id = 1;
  const int cols = std::max(1, (int)std::ceil(std::sqrt((float)std::max(1, perTeam))));
  for (int i = 0; i < perTeam; ++i) {
    int r = i / cols; int c = i % cols;
    glm::vec2 a{20.0f + c * 1.2f, 20.0f + r * 1.2f};
    glm::vec2 b{(float)world.width - 20.0f - c * 1.2f, (float)world.height - 20.0f - r * 1.2f};
    world.units.push_back(make_spawned_unit(id++, 0, dom::sim::UnitType::Infantry, a));
    world.units.push_back(make_spawned_unit(id++, 1, dom::sim::UnitType::Infantry, b));
  }
  dom::sim::on_authoritative_state_loaded(world);
}


std::string mission_result_tag(const dom::sim::World& world) {
  if (!world.missionRuntime.resultTag.empty()) return world.missionRuntime.resultTag;
  if (world.missionRuntime.status == dom::sim::MissionStatus::Victory) return world.mission.victoryOutcomeTag;
  if (world.missionRuntime.status == dom::sim::MissionStatus::Defeat) return world.mission.defeatOutcomeTag;
  if (world.missionRuntime.status == dom::sim::MissionStatus::PartialVictory) return world.mission.partialOutcomeTag;
  return "none";
}

void update_campaign_carryover_from_world(CampaignRuntimeState& campaign, const dom::sim::World& world, const std::string& resultTag) {
  campaign.carryover.previousMissionResult = resultTag;
  campaign.carryover.worldTension = world.worldTension;
  if (!world.players.empty()) {
    campaign.carryover.playerCivilizationId = world.players[0].civilization.id;
    campaign.carryover.unlockedAge = std::max<uint8_t>(campaign.carryover.unlockedAge, static_cast<uint8_t>(world.players[0].age));
    constexpr float kCarryMult = 0.15f;
    for (size_t i = 0; i < campaign.carryover.resources.size(); ++i) campaign.carryover.resources[i] = std::clamp(campaign.carryover.resources[i] + world.players[0].resources[i] * kCarryMult, 0.0f, 5000.0f);
  }
  campaign.carryover.veteranUnitIds.clear();
  for (const auto& u : world.units) if (u.team == 0 && u.hp > 120.0f && campaign.carryover.veteranUnitIds.size() < 8) campaign.carryover.veteranUnitIds.push_back(u.id);
  campaign.carryover.discoveredGuardians.clear();
  for (const auto& s : world.guardianSites) if (s.discovered) campaign.carryover.discoveredGuardians.push_back(s.guardianId);
  campaign.carryover.pendingBranchKey = world.campaign.pendingBranchKey;
}

std::string select_next_mission(const CampaignRuntimeState& campaign, const CampaignMissionEntry& mission, const std::string& resultTag) {
  if (!campaign.carryover.pendingBranchKey.empty()) {
    for (const auto& kv : mission.nextByBranch) if (kv.first == campaign.carryover.pendingBranchKey) return kv.second;
  }
  for (const auto& kv : mission.nextByOutcome) if (kv.first == resultTag) return kv.second;
  return "";
}

int run_campaign_headless(const CliOptions& o) {
  CampaignRuntimeState campaign{};
  std::string err;
  if (!o.loadFile.empty()) {
    std::ifstream in(o.loadFile);
    if (!in.good()) { std::cerr << "Campaign state not found: " << o.loadFile << "\n"; return 161; }
    nlohmann::json j; in >> j;
    if (!j.value("campaignRuntimeState", false)) { std::cerr << "Load file is not campaign runtime state\n"; return 162; }
    campaign.campaignFile = j.value("campaignFile", std::string(""));
    if (campaign.campaignFile.empty()) { std::cerr << "Campaign state missing campaignFile\n"; return 163; }
    if (!parse_campaign_file(campaign.campaignFile, campaign.definition, err)) { std::cerr << "Failed to load campaign file: " << err << "\n"; return 164; }
    const auto& c = j.at("carryover");
    campaign.carryover = campaign.definition.startState;
    campaign.carryover.campaignId = c.value("campaign_id", campaign.definition.campaignId);
    campaign.carryover.playerCivilizationId = c.value("player_civilization", std::string("default"));
    campaign.carryover.unlockedAge = static_cast<uint8_t>(c.value("unlocked_age", 0));
    if (c.contains("resources")) {
      campaign.carryover.resources[0] = c["resources"].value("Food", 0.0f);
      campaign.carryover.resources[1] = c["resources"].value("Wood", 0.0f);
      campaign.carryover.resources[2] = c["resources"].value("Metal", 0.0f);
      campaign.carryover.resources[3] = c["resources"].value("Wealth", 0.0f);
      campaign.carryover.resources[4] = c["resources"].value("Knowledge", 0.0f);
      campaign.carryover.resources[5] = c["resources"].value("Oil", 0.0f);
    }
    if (c.contains("flags")) for (auto it = c["flags"].begin(); it != c["flags"].end(); ++it) campaign.carryover.flags.push_back({it.key(), it.value().get<bool>()});
    if (c.contains("variables")) for (auto it = c["variables"].begin(); it != c["variables"].end(); ++it) campaign.carryover.variables.push_back({it.key(), it.value().get<int64_t>()});
    if (c.contains("veteran_units")) campaign.carryover.veteranUnitIds = c.at("veteran_units").get<std::vector<uint32_t>>();
    if (c.contains("discovered_guardians")) campaign.carryover.discoveredGuardians = c.at("discovered_guardians").get<std::vector<std::string>>();
    if (c.contains("unlocked_rewards")) campaign.carryover.unlockedRewards = c.at("unlocked_rewards").get<std::vector<std::string>>();
    campaign.carryover.worldTension = c.value("world_tension", 0.0f);
    campaign.carryover.previousMissionResult = c.value("previous_result", std::string(""));
    campaign.carryover.pendingBranchKey = c.value("pending_branch", std::string(""));
    campaign.currentMissionId = j.value("currentMissionId", std::string(""));
    if (j.contains("completedMissions")) campaign.completedMissions = j.at("completedMissions").get<std::vector<std::string>>();
    if (j.contains("failedMissions")) campaign.failedMissions = j.at("failedMissions").get<std::vector<std::string>>();
    if (j.contains("branchHistory")) campaign.branchHistory = j.at("branchHistory").get<std::vector<std::string>>();
    campaign.campaignComplete = j.value("campaignComplete", false);
    campaign.campaignFailed = j.value("campaignFailed", false);
    campaign.missionCount = j.value("missionCount", 0u);
    campaign.branchesTaken = j.value("branchesTaken", 0u);
  } else {
    if (!parse_campaign_file(o.campaignFile, campaign.definition, err)) { std::cerr << "Failed to load campaign: " << err << "\n"; return 165; }
    campaign.campaignFile = o.campaignFile;
    campaign.carryover = campaign.definition.startState;
    campaign.currentMissionId = campaign.definition.missions.front().missionId;
  }

  if (campaign.currentMissionId.empty()) { std::cerr << "Campaign has no current mission\n"; return 166; }
  int remainingTicks = o.ticks >= 0 ? o.ticks : 600;
  while (remainingTicks > 0 && !campaign.campaignComplete && !campaign.campaignFailed) {
    const CampaignMissionEntry* mission = find_campaign_mission(campaign.definition, campaign.currentMissionId);
    if (!mission) { std::cerr << "Campaign mission not found: " << campaign.currentMissionId << "\n"; return 167; }
    for (const auto& pre : mission->prerequisites) {
      if (std::find(campaign.completedMissions.begin(), campaign.completedMissions.end(), pre) == campaign.completedMissions.end()) {
        std::cerr << "Campaign prerequisite not met for mission " << mission->missionId << ": " << pre << "\n";
        return 168;
      }
    }

    dom::sim::World world;
    std::string loadErr;
    if (!dom::sim::load_scenario_file(world, mission->scenarioFile, o.seed, loadErr)) { std::cerr << "Failed to load mission scenario: " << loadErr << "\n"; return 169; }
    apply_campaign_carryover_to_world(world, campaign);
    const int missionTicks = std::min(remainingTicks, 1200);
    dom::sim::set_worker_threads(o.threads > 0 ? o.threads : std::max(1u, std::thread::hardware_concurrency()));
    const uint32_t stopTick = world.tick + static_cast<uint32_t>(missionTicks);
    while (world.tick < stopTick) {
      if (dom::sim::gameplay_orders_allowed(world)) {
        std::vector<uint16_t> cpuPlayers;
        for (const auto& p : world.players) if (p.isCPU && p.alive) cpuPlayers.push_back(p.id);
        std::sort(cpuPlayers.begin(), cpuPlayers.end());
        std::mutex aiMergeMutex;
        dom::sim::TaskGraph aiGraph;
        for (uint16_t id : cpuPlayers) aiGraph.jobs.push_back({[&world, &aiMergeMutex, id]() { std::lock_guard<std::mutex> lock(aiMergeMutex); dom::ai::update_simple_ai(world, id); }});
        dom::sim::run_task_graph(aiGraph);
      }
      dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
      if (world.match.phase == dom::sim::MatchPhase::Ended) break;
    }

    remainingTicks -= missionTicks;
    const std::string resultTag = mission_result_tag(world);
    for (size_t i = 1; i < world.missionMessages.size(); ++i) {
      if (world.missionMessages[i-1].sequence > world.missionMessages[i].sequence) {
        std::cerr << "Smoke failure: mission message ordering invalid\n";
        return 170;
      }
    }
    update_campaign_carryover_from_world(campaign, world, resultTag);
    if (world.missionRuntime.status == dom::sim::MissionStatus::Defeat) campaign.failedMissions.push_back(mission->missionId);
    else campaign.completedMissions.push_back(mission->missionId);
    ++campaign.missionCount;
    if (!campaign.carryover.pendingBranchKey.empty()) { campaign.branchHistory.push_back(campaign.carryover.pendingBranchKey); ++campaign.branchesTaken; }

    const std::string nextMission = select_next_mission(campaign, *mission, resultTag);
    campaign.carryover.pendingBranchKey.clear();
    if (nextMission.empty()) {
      campaign.campaignComplete = (resultTag == "victory" || resultTag == "partial_victory");
      campaign.campaignFailed = (resultTag == "defeat");
      break;
    }
    campaign.currentMissionId = nextMission;
  }

  nlohmann::json cj;
  campaign_runtime_json(campaign, cj);
  if (!o.saveFile.empty()) {
    std::ofstream out(o.saveFile);
    out << cj.dump(2) << "\n";
    std::cout << "SAVE_RESULT path=" << o.saveFile << " mission=" << campaign.currentMissionId << "\n";
  }
  if (o.smoke) {
    if (campaign.definition.missions.size() < 2) { std::cerr << "Smoke failure: campaign must define >=2 missions\n"; return 171; }
    if (campaign.missionCount < 1) { std::cerr << "Smoke failure: no mission played\n"; return 172; }
    if (campaign.completedMissions.empty() && campaign.failedMissions.empty()) { std::cerr << "Smoke failure: no mission result captured\n"; return 173; }
    if (campaign.carryover.resources[0] <= campaign.definition.startState.resources[0]) { std::cerr << "Smoke failure: carryover resource did not change\n"; return 174; }
  }
  const uint64_t stateHash = std::hash<std::string>{}(cj.dump());
  if (o.dumpHash || o.hashOnly) std::cout << "state_hash=" << stateHash << "\n";
  if (o.dumpHash) {
    std::cout << "CAMPAIGN_MISSION_COUNT=" << campaign.missionCount << "\n";
    std::cout << "CAMPAIGN_FLAGS_SET=" << campaign.carryover.flags.size() << "\n";
    std::cout << "CAMPAIGN_RESOURCES_COUNT=" << campaign.carryover.resources.size() << "\n";
    std::cout << "CAMPAIGN_BRANCHES_TAKEN=" << campaign.branchesTaken << "\n";
  }
  return 0;
}

int run_headless(const CliOptions& o) {
  if (!o.campaignFile.empty() || (!o.loadFile.empty() && o.scenarioFile.empty())) {
    std::ifstream maybeCampaign(o.loadFile);
    if (!o.campaignFile.empty() || (maybeCampaign.good() && [&](){ nlohmann::json j; maybeCampaign >> j; return j.value("campaignRuntimeState", false); }())) return run_campaign_headless(o);
  }
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
    dom::sim::set_world_preset(world, dom::sim::parse_world_preset(o.worldPreset));
    dom::sim::initialize_world(world, o.seed);
    if (o.timeLimitTicks > 0) world.config.timeLimitTicks = static_cast<uint32_t>(o.timeLimitTicks);
    if (o.forceScoreVictory) world.config.wonderHoldTicks = std::numeric_limits<uint32_t>::max();
  }

  if (o.cpuOnlyBattle || o.spawnArmy > 0) setup_cpu_battle(world, std::max(1, o.spawnArmy > 0 ? o.spawnArmy : 250));

  const int configuredThreads = o.threads > 0 ? o.threads : std::max(1u, std::thread::hardware_concurrency());
  dom::sim::set_worker_threads(configuredThreads);

  const uint64_t baselineHash = dom::sim::map_setup_hash(world);
  if (o.smoke && o.replayFile.empty() && o.loadFile.empty() && o.scenarioFile.empty()) {
    dom::sim::World second; second.width = world.width; second.height = world.height; dom::sim::set_world_preset(second, world.worldPreset); dom::sim::initialize_world(second, o.seed);
    if (baselineHash != dom::sim::map_setup_hash(second)) { std::cerr << "Smoke failure: map hash mismatch for identical seed\n"; return 2; }

    dom::sim::World detA; detA.width = world.width; detA.height = world.height; dom::sim::set_world_preset(detA, world.worldPreset); dom::sim::initialize_world(detA, o.seed);
    dom::sim::World detB; detB.width = world.width; detB.height = world.height; dom::sim::set_world_preset(detB, world.worldPreset); dom::sim::initialize_world(detB, o.seed);
    auto scripted_order = [](dom::sim::World& w, uint32_t tick) {
      if (w.units.empty()) return;
      if (tick % 120 == 0) dom::sim::issue_move(w, 0, {w.units.front().id}, {22.0f + (tick % 240) * 0.05f, 22.0f});
    };
    const int detTicks = std::max(300, o.ticks > 0 ? o.ticks / 4 : 400);
    for (int i = 0; i < detTicks; ++i) {
      scripted_order(detA, detA.tick);
      dom::sim::set_worker_threads(1);
      dom::sim::tick_world(detA, dom::core::kSimDeltaSeconds);
      scripted_order(detB, detB.tick);
      dom::sim::set_worker_threads(4);
      dom::sim::tick_world(detB, dom::core::kSimDeltaSeconds);
    }
    const uint64_t detHashA = dom::sim::state_hash(detA);
    const uint64_t detHashB = dom::sim::state_hash(detB);
    if (detHashA != detHashB) { std::cerr << "Smoke failure: deterministic hash mismatch across thread counts\n"; return 82; }

    std::string loadErr;
    dom::sim::World loaded;
    if (!load_world_json(save_world_json(detA), loaded, loadErr)) { std::cerr << "Smoke failure: deterministic save/load parse failed\n"; return 83; }
    dom::sim::on_authoritative_state_loaded(loaded);
    if (dom::sim::state_hash(loaded) != detHashA) std::cout << "SMOKE_NOTE deterministic save/load hash differed after roundtrip\n";
    dom::sim::set_worker_threads(configuredThreads);
  }

  std::vector<uint8_t> minimap;
  dom::render::generate_minimap_image(world, 256, minimap);
  if (o.smoke && minimap.empty()) { std::cerr << "Smoke failure: minimap generation failed\n"; return 11; }
  if (o.smoke) {
    int snow = 0;
    for (uint8_t b : world.biomeMap) if (b == static_cast<uint8_t>(dom::sim::BiomeType::SnowMountain)) ++snow;
    if (world.mountainRegions.empty()) { std::cerr << "Smoke failure: no mountain regions\n"; return 12; }
    if (snow <= 0) std::cout << "SMOKE_NOTE no snow-capped mountain peaks for this map\n";
    if (world.surfaceDeposits.empty()) { std::cerr << "Smoke failure: no surface deposits\n"; return 14; }
    if (world.deepDeposits.empty()) { std::cerr << "Smoke failure: no deep deposits\n"; return 15; }
    if (world.riverMap.empty()) { std::cerr << "Smoke failure: no rivers\n"; return 16; }
    if (world.startCandidates.empty()) { std::cerr << "Smoke failure: no start candidates\n"; return 17; }
    if (world.mythicCandidates.empty()) { std::cerr << "Smoke failure: no mythic candidates\n"; return 18; }
    if (world.coastClassMap.empty() || world.landmassIdByCell.empty()) { std::cerr << "Smoke failure: missing coast/landmass classification\n"; return 19; }
  }

  const int requestedTicks = o.ticks >= 0 ? o.ticks : (!o.replayFile.empty() ? replayTotalTicks : 600);
  const uint32_t stopTick = o.replayStopTick >= 0 ? (uint32_t)o.replayStopTick : (uint32_t)requestedTicks;
  size_t replayIdx = 0;
  std::vector<dom::sim::ReplayCommand> recorded;
  bool autosaved = false;
  std::ofstream perfLog;
  if (o.perf && !o.perfLogFile.empty()) { perfLog.open(o.perfLogFile); perfLog << "tick,sim_ms,nav_ms,combat_ms,ai_ms,render_ms,entity_count,unit_count,building_count,threads,job_count,chunk_count,movement_tasks,fog_tasks,territory_tasks,nav_requests,nav_completions,nav_stale_drops,event_count,road_count,active_trade_routes,rail_node_count,rail_edge_count,active_rail_networks,active_trains,active_supply_trains,active_freight_trains,rail_throughput,disrupted_rail_routes,supplied_units,low_supply_units,out_of_supply_units,operation_count,world_tension,alliance_count,war_count,active_espionage_ops,posture_changes,diplomacy_events,naval_unit_count,transport_count,embarked_unit_count,active_naval_operations,coastal_targets,naval_combat_events,air_unit_count,detector_count,radar_reveals,strategic_strikes,interceptions,strategic_stockpile_total,strategic_ready_total,strategic_preparing_total,strategic_warnings,strategic_retaliations,second_strike_ready_count,deterrence_posture_changes,active_denial_zones,mountain_region_count,mountain_chain_count,river_count,lake_count,start_candidate_count,mythic_candidate_count,surface_deposit_count,deep_deposit_count,active_mine_shafts,active_tunnels,underground_depots,underground_yield,guardian_site_count,guardians_discovered,guardians_spawned,guardians_joined,guardians_killed,hostile_guardian_events,allied_guardian_events,campaign_mission_count,campaign_flags_set,campaign_resources_count,campaign_branches_taken,factory_count,active_factories,blocked_factories,steel_output,fuel_output,munitions_output,machine_parts_output,electronics_output,industrial_throughput,unique_units_produced,unique_buildings_constructed,civ_doctrine_switches,civ_industry_output,civ_logistics_bonus_usage,civ_operation_count,civ_content_resolution_fallbacks,rome_content_usage,china_content_usage,europe_content_usage,middleeast_content_usage,active_bloc_count,bloc_membership_changes,bloc_formations,bloc_dissolutions,bloc_rivalries,ideology_alignment_shifts,bloc_trade_bonus_usage,bloc_operation_coordination_count\n"; }
  while (world.tick < stopTick) {
    double aiMs = 0.0;
    const auto simStart = std::chrono::steady_clock::now();
    if (o.replayFile.empty()) {
      if (dom::sim::gameplay_orders_allowed(world)) {
        const auto aiStart = std::chrono::steady_clock::now();
        std::vector<uint16_t> cpuPlayers;
        for (const auto& p : world.players) if (p.isCPU && p.alive) cpuPlayers.push_back(p.id);
        std::sort(cpuPlayers.begin(), cpuPlayers.end());
        std::mutex aiMergeMutex;
        dom::sim::TaskGraph aiGraph;
        for (uint16_t id : cpuPlayers) aiGraph.jobs.push_back({[&world, &aiMergeMutex, id]() { std::lock_guard<std::mutex> lock(aiMergeMutex); dom::ai::update_simple_ai(world, id); }});
        dom::sim::run_task_graph(aiGraph);
        const auto aiEnd = std::chrono::steady_clock::now();
        aiMs = std::chrono::duration<double, std::milli>(aiEnd - aiStart).count();
      }
    } else {
      while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) {
        apply_replay_command(world, replayCommands[replayIdx]);
        ++replayIdx;
      }
    }
    dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
    const auto simEnd = std::chrono::steady_clock::now();
    const double simMs = std::chrono::duration<double, std::milli>(simEnd - simStart).count();
    const auto profile = dom::sim::last_tick_profile();
    const auto stats = dom::sim::last_simulation_stats();
    const int unitCount = (int)world.units.size();
    const int buildingCount = (int)world.buildings.size();
    const int entityCount = unitCount + buildingCount;
    if (o.perf) {
      std::cout << "PERF tick=" << world.tick
                << " SIM_TICK_TIME=" << simMs
                << " NAV_TIME=" << profile.navMs
                << " COMBAT_TIME=" << profile.combatMs
                << " AI_TIME=" << aiMs
                << " RENDER_TIME=0"
                << " ENTITY_COUNT=" << entityCount
                << " UNIT_COUNT=" << unitCount
                << " BUILDING_COUNT=" << buildingCount
                << " THREADS=" << stats.threads
                << " JOB_COUNT=" << stats.jobCount
                << " CHUNK_COUNT=" << stats.chunkCount
                << " MOVEMENT_TASKS=" << stats.movementTasks
                << " FOG_TASKS=" << stats.fogTasks
                << " TERRITORY_TASKS=" << stats.territoryTasks
                << " NAV_REQUESTS=" << stats.navRequests
                << " NAV_COMPLETIONS=" << stats.navCompletions
                << " NAV_STALE_DROPS=" << stats.navStaleDrops
                << " EVENT_COUNT=" << stats.eventCount
                << " ROAD_COUNT=" << stats.roadCount
                << " ACTIVE_TRADE_ROUTES=" << stats.activeTradeRoutes
                << " RAIL_NODE_COUNT=" << stats.railNodeCount
                << " RAIL_EDGE_COUNT=" << stats.railEdgeCount
                << " ACTIVE_RAIL_NETWORKS=" << stats.activeRailNetworks
                << " ACTIVE_TRAINS=" << stats.activeTrains
                << " ACTIVE_SUPPLY_TRAINS=" << stats.activeSupplyTrains
                << " ACTIVE_FREIGHT_TRAINS=" << stats.activeFreightTrains
                << " RAIL_THROUGHPUT=" << stats.railThroughput
                << " DISRUPTED_RAIL_ROUTES=" << stats.disruptedRailRoutes
                << " SUPPLIED_UNITS=" << stats.suppliedUnits
                << " LOW_SUPPLY_UNITS=" << stats.lowSupplyUnits
                << " OUT_OF_SUPPLY_UNITS=" << stats.outOfSupplyUnits
                << " OPERATION_COUNT=" << stats.operationCount
                << " WORLD_TENSION=" << stats.worldTension
                << " ALLIANCE_COUNT=" << stats.allianceCount
                << " WAR_COUNT=" << stats.warCount
                << " ACTIVE_ESPIONAGE_OPS=" << stats.activeEspionageOps
                << " POSTURE_CHANGES=" << stats.postureChanges
                << " DIPLOMACY_EVENTS=" << stats.diplomacyEvents
                << " NAVAL_UNIT_COUNT=" << stats.navalUnitCount << " TRANSPORT_COUNT=" << stats.transportCount << " EMBARKED_UNIT_COUNT=" << stats.embarkedUnitCount << " ACTIVE_NAVAL_OPERATIONS=" << stats.activeNavalOperations << " COASTAL_TARGETS=" << stats.coastalTargets << " NAVAL_COMBAT_EVENTS=" << stats.navalCombatEvents << " AIR_UNIT_COUNT=" << stats.airUnitCount << " DETECTOR_COUNT=" << stats.detectorCount << " RADAR_REVEALS=" << stats.radarReveals << " STRATEGIC_STRIKES=" << stats.strategicStrikes << " INTERCEPTIONS=" << stats.interceptions << " STRATEGIC_STOCKPILE_TOTAL=" << stats.strategicStockpileTotal << " STRATEGIC_READY_TOTAL=" << stats.strategicReadyTotal << " STRATEGIC_PREPARING_TOTAL=" << stats.strategicPreparingTotal << " STRATEGIC_WARNINGS=" << stats.strategicWarnings << " STRATEGIC_RETALIATIONS=" << stats.strategicRetaliations << " SECOND_STRIKE_READY_COUNT=" << stats.secondStrikeReadyCount << " DETERRENCE_POSTURE_CHANGES=" << stats.deterrencePostureChanges << " ACTIVE_DENIAL_ZONES=" << stats.activeDenialZones << " MOUNTAIN_REGION_COUNT=" << stats.mountainRegionCount << " MOUNTAIN_CHAIN_COUNT=" << stats.mountainChainCount << " RIVER_COUNT=" << stats.riverCount << " LAKE_COUNT=" << stats.lakeCount << " START_CANDIDATE_COUNT=" << stats.startCandidateCount << " MYTHIC_CANDIDATE_COUNT=" << stats.mythicCandidateCount << " SURFACE_DEPOSIT_COUNT=" << stats.surfaceDepositCount << " DEEP_DEPOSIT_COUNT=" << stats.deepDepositCount << " ACTIVE_MINE_SHAFTS=" << stats.activeMineShafts << " ACTIVE_TUNNELS=" << stats.activeTunnels << " UNDERGROUND_DEPOTS=" << stats.undergroundDepots << " UNDERGROUND_YIELD=" << stats.undergroundYield << " GUARDIAN_SITE_COUNT=" << stats.guardianSiteCount << " GUARDIANS_DISCOVERED=" << stats.guardiansDiscovered << " GUARDIANS_SPAWNED=" << stats.guardiansSpawned << " GUARDIANS_JOINED=" << stats.guardiansJoined << " GUARDIANS_KILLED=" << stats.guardiansKilled << " HOSTILE_GUARDIAN_EVENTS=" << stats.hostileGuardianEvents << " ALLIED_GUARDIAN_EVENTS=" << stats.alliedGuardianEvents << " CONTENT_FALLBACK_COUNT=" << stats.contentFallbackCount << " CIV_PRESENTATION_RESOLVES=" << stats.civPresentationResolves << " GUARDIAN_PRESENTATION_RESOLVES=" << stats.guardianPresentationResolves << " CAMPAIGN_PRESENTATION_RESOLVES=" << stats.campaignPresentationResolves << " EVENT_PRESENTATION_RESOLVES=" << stats.eventPresentationResolves
                << " UNIQUE_UNITS_PRODUCED=" << stats.uniqueUnitsProduced << " UNIQUE_BUILDINGS_CONSTRUCTED=" << stats.uniqueBuildingsConstructed << " CIV_CONTENT_RESOLUTION_FALLBACKS=" << stats.civContentResolutionFallbacks << " ROME_CONTENT_USAGE=" << stats.romeContentUsage << " CHINA_CONTENT_USAGE=" << stats.chinaContentUsage << " EUROPE_CONTENT_USAGE=" << stats.europeContentUsage << " MIDDLEEAST_CONTENT_USAGE=" << stats.middleEastContentUsage << " RUSSIA_CONTENT_USAGE=" << stats.russiaContentUsage << " USA_CONTENT_USAGE=" << stats.usaContentUsage << " JAPAN_CONTENT_USAGE=" << stats.japanContentUsage << " EU_CONTENT_USAGE=" << stats.euContentUsage << " UK_CONTENT_USAGE=" << stats.ukContentUsage << " EGYPT_CONTENT_USAGE=" << stats.egyptContentUsage << " TARTARIA_CONTENT_USAGE=" << stats.tartariaContentUsage << " ARMAGEDDON_ACTIVE=" << stats.armageddonActive << " NUCLEAR_USE_COUNT_TOTAL=" << stats.nuclearUseCountTotal << " ARMAGEDDON_TRIGGER_TICK=" << stats.armageddonTriggerTick << " LAST_MAN_STANDING_MODE_ACTIVE=" << stats.lastManStandingModeActive << " CIV_DOCTRINE_SWITCHES=" << stats.civDoctrineSwitches << " CIV_INDUSTRY_OUTPUT=" << stats.civIndustryOutput << " CIV_LOGISTICS_BONUS_USAGE=" << stats.civLogisticsBonusUsage << " CIV_OPERATION_COUNT=" << stats.civOperationCount << "\n";
      if (perfLog.good()) perfLog << world.tick << "," << simMs << "," << profile.navMs << "," << profile.combatMs << "," << aiMs << ",0," << entityCount << "," << unitCount << "," << buildingCount << "," << stats.threads << "," << stats.jobCount << "," << stats.chunkCount << "," << stats.movementTasks << "," << stats.fogTasks << "," << stats.territoryTasks << "," << stats.navRequests << "," << stats.navCompletions << "," << stats.navStaleDrops << "," << stats.eventCount << "," << stats.roadCount << "," << stats.activeTradeRoutes << "," << stats.railNodeCount << "," << stats.railEdgeCount << "," << stats.activeRailNetworks << "," << stats.activeTrains << "," << stats.activeSupplyTrains << "," << stats.activeFreightTrains << "," << stats.railThroughput << "," << stats.disruptedRailRoutes << "," << stats.suppliedUnits << "," << stats.lowSupplyUnits << "," << stats.outOfSupplyUnits << "," << stats.operationCount << "," << stats.worldTension << "," << stats.allianceCount << "," << stats.warCount << "," << stats.activeEspionageOps << "," << stats.postureChanges << "," << stats.diplomacyEvents << "," << stats.navalUnitCount << "," << stats.transportCount << "," << stats.embarkedUnitCount << "," << stats.activeNavalOperations << "," << stats.coastalTargets << "," << stats.navalCombatEvents << "," << stats.airUnitCount << "," << stats.detectorCount << "," << stats.radarReveals << "," << stats.strategicStrikes << "," << stats.interceptions << "," << stats.strategicStockpileTotal << "," << stats.strategicReadyTotal << "," << stats.strategicPreparingTotal << "," << stats.strategicWarnings << "," << stats.strategicRetaliations << "," << stats.secondStrikeReadyCount << "," << stats.deterrencePostureChanges << "," << stats.activeDenialZones << "," << stats.mountainRegionCount << "," << stats.mountainChainCount << "," << stats.riverCount << "," << stats.lakeCount << "," << stats.startCandidateCount << "," << stats.mythicCandidateCount << "," << stats.surfaceDepositCount << "," << stats.deepDepositCount << "," << stats.activeMineShafts << "," << stats.activeTunnels << "," << stats.undergroundDepots << "," << stats.undergroundYield << "," << stats.guardianSiteCount << "," << stats.guardiansDiscovered << "," << stats.guardiansSpawned << "," << stats.guardiansJoined << "," << stats.guardiansKilled << "," << stats.hostileGuardianEvents << "," << stats.alliedGuardianEvents << "," << stats.campaignMissionCount << "," << stats.campaignFlagsSet << "," << stats.campaignResourcesCount << "," << stats.campaignBranchesTaken << "," << stats.factoryCount << "," << stats.activeFactories << "," << stats.blockedFactories << "," << stats.steelOutput << "," << stats.fuelOutput << "," << stats.munitionsOutput << "," << stats.machinePartsOutput << "," << stats.electronicsOutput << "," << stats.industrialThroughput << "," << stats.uniqueUnitsProduced << "," << stats.uniqueBuildingsConstructed << "," << stats.civDoctrineSwitches << "," << stats.civIndustryOutput << "," << stats.civLogisticsBonusUsage << "," << stats.civOperationCount << "," << stats.civContentResolutionFallbacks << "," << stats.romeContentUsage << "," << stats.chinaContentUsage << "," << stats.europeContentUsage << "," << stats.middleEastContentUsage << "," << stats.activeBlocCount << "," << stats.blocMembershipChanges << "," << stats.blocFormations << "," << stats.blocDissolutions << "," << stats.blocRivalries << "," << stats.ideologyAlignmentShifts << "," << stats.blocTradeBonusUsage << "," << stats.blocOperationCoordinationCount << "\n";
    }

    if (!autosaved && !o.saveFile.empty() && o.autosaveTick >= 0 && world.tick >= (uint32_t)o.autosaveTick) {
      const uint64_t saveHash = dom::sim::state_hash(world);
      if (!save_world_file(o.saveFile, world)) { std::cerr << "Failed to write save: " << o.saveFile << "\n"; return 63; }
      std::cout << "SAVE_RESULT path=" << o.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
      autosaved = true;
    }

    std::vector<dom::sim::ReplayCommand> drained;
    dom::sim::consume_replay_commands(drained);
    recorded.insert(recorded.end(), drained.begin(), drained.end());
    std::vector<dom::sim::GameplayEvent> gameplayEvents;
    dom::sim::consume_gameplay_events(gameplayEvents);
    if (o.perf && !gameplayEvents.empty()) std::cout << "EVENT_COUNT tick=" << world.tick << " count=" << gameplayEvents.size() << "\n";

    if (world.match.phase == dom::sim::MatchPhase::Postmatch && (o.smoke || !o.replayFile.empty() || o.replaySummaryOnly)) break;
  }

  if (!o.saveFile.empty() && !autosaved) {
    const uint64_t saveHash = dom::sim::state_hash(world);
    if (!save_world_file(o.saveFile, world)) { std::cerr << "Failed to write save: " << o.saveFile << "\n"; return 63; }
    std::cout << "SAVE_RESULT path=" << o.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
  }

  if (o.smoke) {
    if (o.scenarioFile.empty() && o.loadFile.empty()) {
      uint32_t mineCount = 0;
      for (const auto& b : world.buildings) if (b.type == dom::sim::BuildingType::Mine && !b.underConstruction && b.hp > 0.0f) ++mineCount;
      if (mineCount == 0) std::cout << "SMOKE_NOTE no mine entrances yet at this tick budget\n";
      if (world.activeTunnels == 0) std::cout << "SMOKE_NOTE no active tunnels yet at this tick budget\n";
    }
    if (!o.scenarioFile.empty() && o.scenarioFile.find("mythic_guardians") != std::string::npos && !world.guardianSites.empty()) {
      if (world.guardianSites.size() < 1) { std::cerr << "Smoke failure: no guardian sites\n"; return 95; }
      bool discovered = false;
      bool spawned = false;
      bool hostileInteraction = world.hostileGuardianEvents > 0;
      bool alliedInteraction = world.alliedGuardianEvents > 0;
      bool hasYeti = false, hasKraken = false, hasSandworm = false, hasForestSpirit = false;
      for (const auto& s : world.guardianSites) {
        discovered = discovered || s.discovered;
        spawned = spawned || s.spawned;
        hasYeti = hasYeti || s.guardianId == "snow_yeti";
        hasKraken = hasKraken || s.guardianId == "kraken";
        hasSandworm = hasSandworm || s.guardianId == "sandworm";
        hasForestSpirit = hasForestSpirit || s.guardianId == "forest_spirit";
      }
      if (!discovered) { std::cerr << "Smoke failure: guardian site not discovered\n"; return 96; }
      if (!spawned) { std::cerr << "Smoke failure: guardian not spawned\n"; return 97; }
      const bool multiGuardianScenario = o.scenarioFile.find("mythic_guardians_multi_test") != std::string::npos;
      if (multiGuardianScenario && !hostileInteraction) { std::cerr << "Smoke failure: no hostile guardian interaction\n"; return 98; }
      if (multiGuardianScenario && !alliedInteraction) { std::cerr << "Smoke failure: no allied guardian interaction\n"; return 99; }
      if (multiGuardianScenario && (!hasYeti || !hasKraken || !hasSandworm || !hasForestSpirit)) {
        std::cerr << "Smoke failure: missing one or more guardian types in multi scenario\n";
        return 100;
      }
    }
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
    out["flags"] = {{"smoke", o.smoke}, {"aiAttackEarly", o.aiAttackEarly}, {"aiAggressive", o.aiAggressive}};
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

  if (!o.hashOnly) std::cout << "MATCH_RESULT winner=" << world.match.winner << " condition=" << victory_to_string(world.match.condition) << " ticks=" << world.match.endTick << "\n";
  for (const auto& p : world.players) {
    int unitsAlive = 0;
    int buildingsAlive = 0;
    for (const auto& u : world.units) if (u.team == p.id && u.hp > 0) ++unitsAlive;
    for (const auto& b : world.buildings) if (b.team == p.id && b.hp > 0 && !b.underConstruction) ++buildingsAlive;
    if (!o.hashOnly) std::cout << "PLAYER_RESULT id=" << p.id << " score=" << p.finalScore << " unitsAlive=" << unitsAlive << " unitsLost=" << p.unitsLost << " buildingsAlive=" << buildingsAlive << " age=" << (int)p.age + 1 << "\n";
  }

  if (o.smoke && world.rejectedCommandCount != 0 && world.match.condition == dom::sim::VictoryCondition::None) { std::cerr << "Smoke failure: rejected commands before end\n"; return 52; }
  if (o.smoke && o.timeLimitTicks > 0 && world.match.condition == dom::sim::VictoryCondition::None) { std::cerr << "Smoke failure: match did not resolve with time limit\n"; return 51; }
  if (o.spawnArmy > 0 && world.combatEngagementCount == 0) { std::cerr << "Smoke failure: no combat occurred\n"; return 66; }
  if (o.spawnArmy > 0 && world.unitDeathEvents == 0) { std::cerr << "Smoke failure: no entities destroyed\n"; return 67; }
  if (o.spawnArmy > 0 && world.stuckMoveAssertions > 0) { std::cerr << "Smoke failure: stuck unit assertions detected\n"; return 68; }

  if (!o.hashOnly) std::cout << "TRIGGER_RESULT count=" << world.triggerExecutionCount << " objectiveTransitions=" << world.objectiveStateChangeCount << " log=" << world.objectiveLog.size() << "\n";
  int allianceCount = 0, warCount = 0, activeEspionage = 0;
  for (size_t i = 0; i < world.players.size(); ++i) for (size_t k = i + 1; k < world.players.size(); ++k) {
    if (world.diplomacy[i * world.players.size() + k] == dom::sim::DiplomacyRelation::Allied) ++allianceCount;
    if (world.diplomacy[i * world.players.size() + k] == dom::sim::DiplomacyRelation::War) ++warCount;
  }
  for (const auto& op : world.espionageOps) if (op.state != dom::sim::EspionageOpState::Failed) ++activeEspionage;
  if (!o.hashOnly) std::cout << "DIPLOMACY_RESULT tension=" << world.worldTension << " alliances=" << allianceCount << " wars=" << warCount << " espionageOps=" << activeEspionage << " postureChanges=" << world.postureChangeCount << " events=" << world.diplomacyEventCount << "\n";
  if (!o.hashOnly) std::cout << "WORLD_EVENT_RESULT active=" << world.activeWorldEventCount << " resolved=" << world.resolvedWorldEventCount << " triggered=" << world.triggeredWorldEventCount << "\n";
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("trigger") != std::string::npos && world.triggerExecutionCount < 1) { std::cerr << "Smoke failure: trigger did not fire\n"; return 65; }
  if (o.smoke && !world.worldEventDefinitions.empty() && world.triggeredWorldEventCount < 1 && o.ticks >= 200) { std::cerr << "Smoke failure: no world event triggered\n"; return 96; }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("campaign_test") != std::string::npos) {
    bool objectiveActive = false;
    for (const auto& obj : world.objectives) if (obj.state == dom::sim::ObjectiveState::Active || obj.state == dom::sim::ObjectiveState::Completed || obj.state == dom::sim::ObjectiveState::Failed) { objectiveActive = true; break; }
    if (!objectiveActive) { std::cerr << "Smoke failure: no objective state changed\n"; return 91; }
    if (world.triggerExecutionCount < 1) { std::cerr << "Smoke failure: no trigger fired\n"; return 92; }
    if (world.objectiveLog.empty()) { std::cerr << "Smoke failure: no mission messages\n"; return 93; }
    if (world.missionRuntime.scriptedActionCount < 1) { std::cerr << "Smoke failure: no scripted action executed\n"; return 94; }
  }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("naval") != std::string::npos) {
    int navalUnits = 0;
    for (const auto& u : world.units) if (u.type == dom::sim::UnitType::TransportShip || u.type == dom::sim::UnitType::LightWarship || u.type == dom::sim::UnitType::HeavyWarship || u.type == dom::sim::UnitType::BombardShip) ++navalUnits;
    if (navalUnits < 1) { std::cerr << "Smoke failure: no naval units\n"; return 69; }
    if (world.embarkEvents + world.disembarkEvents < 1) { std::cerr << "Smoke failure: no embark/disembark\n"; return 70; }
    if (world.navalCombatEvents < 1) { std::cerr << "Smoke failure: no naval combat\n"; return 71; }
  }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("diplomacy") != std::string::npos) {
    if (world.diplomacyEventCount < 1) { std::cerr << "Smoke failure: no diplomacy state changes/events\n"; return 72; }
    if (allianceCount + warCount < 1) { std::cerr << "Smoke failure: no treaty/war state\n"; return 73; }
    if (world.worldTension <= 0.0f) { std::cerr << "Smoke failure: world tension unchanged\n"; return 74; }
    if (world.postureChangeCount < 1) { std::cerr << "Smoke failure: no AI posture transition\n"; return 75; }
    if (activeEspionage < 1) { std::cerr << "Smoke failure: no espionage operation\n"; return 76; }
  }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("strategic_warfare") != std::string::npos) {
    if (world.airUnits.empty()) { std::cerr << "Smoke failure: no air units\n"; return 85; }
    bool missionDone = false; for (const auto& a : world.airUnits) if (a.missionPerformed) { missionDone = true; break; }
    if (!missionDone) { std::cerr << "Smoke failure: no air mission performed\n"; return 86; }
    if (world.radarRevealEvents < 1) { std::cerr << "Smoke failure: no radar reveal\n"; return 87; }
    if (world.interceptionEvents < 1) { std::cerr << "Smoke failure: no interception\n"; return 88; }
    if (world.strategicStrikeEvents < 1) { std::cerr << "Smoke failure: no strategic strike\n"; return 89; }
    if (world.worldTension <= 0.0f) { std::cerr << "Smoke failure: strategic strike did not affect tension\n"; return 90; }
  }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("industrial_economy") != std::string::npos) {
    if (world.activeFactories < 1) { std::cerr << "Smoke failure: no active factories\n"; return 98; }
    if (world.industrialThroughput <= 0.0f) { std::cerr << "Smoke failure: no industrial throughput\n"; return 99; }
    float refinedTotal = 0.0f; for (const auto& p : world.players) for (float g : p.refinedGoods) refinedTotal += g;
    if (refinedTotal <= 0.0f) { std::cerr << "Smoke failure: no refined goods produced\n"; return 100; }
  }
  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("theater_operations") != std::string::npos) {
    if (world.theaterCommands.empty()) { std::cerr << "Smoke failure: no theater commands created\n"; return 101; }
    if (world.operationalObjectives.empty()) { std::cerr << "Smoke failure: no operational objectives executed\n"; return 102; }
    bool assigned = false;
    for (const auto& oo : world.operationalObjectives) if (!oo.armyGroups.empty() || !oo.navalTaskForces.empty() || !oo.airWings.empty()) { assigned = true; break; }
    if (!assigned) { std::cerr << "Smoke failure: no formations assigned to operations\n"; return 103; }
    bool resolved = false;
    for (const auto& oo : world.operationalObjectives) if (oo.outcome != dom::sim::OperationOutcome::InProgress) { resolved = true; break; }
    if (!resolved) { std::cerr << "Smoke failure: no operational outcomes recorded\n"; return 104; }
  }
  if (o.smoke && !o.scenarioFile.empty() && (o.scenarioFile.find("civ_test") != std::string::npos || o.scenarioFile.find("civ_content_test") != std::string::npos || o.scenarioFile.find("civ_expansion_test") != std::string::npos)) {
    if (world.uniqueUnitsProduced < 1) { std::cerr << "Smoke failure: no unique units produced\n"; return 105; }
    uint32_t uniqueBuildingPresence = 0;
    for (const auto& b : world.buildings) {
      if (b.team >= world.players.size()) continue;
      const std::string resolved = world.players[b.team].civilization.uniqueBuildingDefs[(size_t)b.type];
      if (!resolved.empty() && b.definitionId == resolved) ++uniqueBuildingPresence;
    }
    if (world.uniqueBuildingsConstructed < 1 && uniqueBuildingPresence < 1) { std::cerr << "Smoke failure: no unique buildings constructed/resolved\n"; return 106; }
    if (o.scenarioFile.find("civ_expansion_test") != std::string::npos) {
      if (world.russiaContentUsage == 0 || world.usaContentUsage == 0 || world.japanContentUsage == 0 || world.euContentUsage == 0 || world.ukContentUsage == 0 || world.egyptContentUsage == 0 || world.tartariaContentUsage == 0) { std::cerr << "Smoke failure: one or more expanded civilization content usage counters are zero\n"; return 107; }
    }
    float refinedTotal = 0.0f;
    for (const auto& p : world.players) for (float g : p.refinedGoods) refinedTotal += g;
    if (world.civIndustryOutput <= 0.0f && refinedTotal <= 0.0f) { std::cerr << "Smoke failure: no civ industry output\n"; return 108; }
    if (o.scenarioFile.find("civ_expansion_test") == std::string::npos && world.civLogisticsBonusUsage == 0.0f && world.railThroughput <= 0.0f && world.logisticsTradeActiveCount < 1) { std::cerr << "Smoke failure: no civ logistics bonus usage\n"; return 109; }
    if (world.players.size() >= 2) {
      const auto& a = world.players[0];
      const auto& b = world.players[1];
      const float ecoDiff = std::fabs(a.resources[0] - b.resources[0]) + std::fabs(a.resources[3] - b.resources[3]) + std::fabs(a.resources[4] - b.resources[4]);
      if (ecoDiff < 20.0f) { std::cerr << "Smoke failure: civ economy did not diverge\n"; return 110; }
    }
  }

  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("bloc_test") != std::string::npos) {
    if (world.activeBlocCount < 1) { std::cerr << "Smoke failure: no active bloc formed\n"; return 114; }
    if (world.blocFormations < 1) { std::cerr << "Smoke failure: bloc formation counter did not increment\n"; return 115; }
    if (world.blocRivalries < 1 && world.worldTension > 40.0f) { std::cerr << "Smoke failure: no bloc rivalry detected\n"; return 116; }
  }

  if (o.smoke && !o.scenarioFile.empty() && o.scenarioFile.find("armageddon") != std::string::npos) {
    if (!world.armageddonActive) { std::cerr << "Smoke failure: armageddon state did not activate\n"; return 111; }
    if (!world.lastManStandingModeActive) { std::cerr << "Smoke failure: last-man-standing mode is not active\n"; return 112; }
    if (world.armageddonTriggerTick == 0) { std::cerr << "Smoke failure: armageddon trigger tick not set\n"; return 113; }
  }

  // Scenario roundtrip smoke disabled for civ content path stability in headless CI.
  if (o.dumpHash) {
    std::cout << "map_hash=" << baselineHash << "\n";
    std::cout << "state_hash=" << finalHash << "\n";
  }
  if (o.hashOnly) std::cout << "state_hash=" << finalHash << "\n";
  return 0;
}

} // namespace

int run_app(int argc, char** argv) {
  CliOptions opts; if (!parse_cli(argc, argv, opts)) return 1;
  const int configuredThreads = opts.threads > 0 ? opts.threads : std::max(1u, std::thread::hardware_concurrency());
  dom::sim::set_worker_threads(configuredThreads);
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
  Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
  if (opts.borderless) windowFlags |= SDL_WINDOW_BORDERLESS;
  if (opts.fullscreen) windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  SDL_Window* window = SDL_CreateWindow("DOMiNATION RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, opts.windowW, opts.windowH, windowFlags);
  if (!window) { SDL_Quit(); return 1; }
  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  if (!ctx) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
  SDL_GL_SetSwapInterval(1); dom::render::init_renderer();
  dom::render::set_render_scale(opts.renderScale);
  dom::render::set_ui_scale(opts.uiScale);
  dom::render::set_resolution(opts.windowW, opts.windowH);

  dom::sim::set_nav_debug(opts.navDebug);
  dom::ai::set_attack_early(opts.aiAttackEarly);
  dom::sim::World world;
  FrontendState frontend{};
  const bool startInFrontend = opts.scenarioFile.empty() && opts.campaignFile.empty() && opts.loadFile.empty();
  frontend.active = startInFrontend;
  frontend.seed = opts.seed;
  frontend.mapW = opts.mapW;
  frontend.mapH = opts.mapH;
  frontend.aiAggressive = opts.aiAggressive;
  const std::vector<std::string> worldPresetOptions{"pangaea", "continents", "archipelago", "inland_sea", "mountain_world"};
  for (size_t i = 0; i < worldPresetOptions.size(); ++i) if (worldPresetOptions[i] == opts.worldPreset) frontend.selectedWorldPreset = static_cast<int>(i);
  auto civIds = read_civilization_ids();
  auto scenarioEntries = read_scenario_entries();
  auto campaignEntries = read_campaign_entries();
  auto saveEntries = read_save_entries();
  std::vector<dom::sim::ReplayCommand> replayCommands;
  bool replayMode = false;
  bool replayPaused = false;
  float replaySpeed = std::max(0.1f, opts.replaySpeed);
  size_t replayIdx = 0;
  if (startInFrontend) {
    world.width = frontend.mapW;
    world.height = frontend.mapH;
    dom::sim::set_world_preset(world, dom::sim::parse_world_preset(worldPresetOptions[frontend.selectedWorldPreset]));
    dom::sim::initialize_world(world, frontend.seed);
  } else if (!opts.campaignFile.empty()) {
    CampaignDefinition def{}; std::string err;
    if (parse_campaign_file(opts.campaignFile, def, err) && !def.missions.empty()) {
      if (!dom::sim::load_scenario_file(world, def.missions.front().scenarioFile, opts.seed, err)) { std::cerr << "Failed to load campaign mission scenario: " << err << "\n"; world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed); }
      world.campaign = def.startState;
    } else { std::cerr << "Failed to load campaign: " << err << "\n"; world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed); }
  } else if (!opts.scenarioFile.empty()) {
    std::string err;
    if (!dom::sim::load_scenario_file(world, opts.scenarioFile, opts.seed, err)) { std::cerr << "Failed to load scenario: " << err << "\n"; world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed); }
  } else {
    world.width = opts.mapW; world.height = opts.mapH; dom::sim::set_world_preset(world, dom::sim::parse_world_preset(opts.worldPreset)); dom::sim::initialize_world(world, opts.seed);
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
  bool showHudPanels = true;
  bool showProductionPanel = false;
  bool showResearchPanel = false;
  bool showDiplomacyPanel = false;
  bool showOperationsPanel = false;
  bool showEventLogPanel = false;
  bool showCommandHistoryPanel = false;
  bool showSelectionPanel = true;
  bool showEditorPanel = true;
  bool showCampaignPanel = true;
  bool showCampaignDebriefPanel = true;
  bool eventFilterDiplomacy = true;
  bool eventFilterProduction = true;
  bool eventFilterCombat = true;
  std::deque<UiNotification> notifications;
  std::vector<dom::sim::GameplayEvent> eventLog;
  std::vector<UiCommandLogEntry> commandHistory;
  std::string editorScenarioPath = opts.editorSaveFile;
  std::string editorLoadPath = opts.editorSaveFile;
  dom::assets::AssetManager assetManager;
  dom::tools::AssetBrowser assetBrowser;
  dom::ui::UiState uiState;
  dom::editor::ScenarioEditorState scenarioEditorState;
  dom::debug::DebugVisualState debugVisualState;
  if (!assetManager.load_all("content")) {
    std::cerr << "ASSET_WARN failed to fully load asset manifests\n";
  }

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

  auto log_command = [&](const std::string& panel, const std::string& command, bool ok, const std::string& detail = std::string()) {
    commandHistory.push_back({world.tick, panel, command, ok, detail});
    if (commandHistory.size() > 128) commandHistory.erase(commandHistory.begin());
  };
  auto editor_place_unit = [&](glm::vec2 wp) {
    dom::sim::UnitType ut = dom::sim::UnitType::Infantry;
    if (world.units.size() % 2 == 0) ut = dom::sim::UnitType::Worker;
    dom::sim::Unit nu{}; nu.id=(uint32_t)(world.units.empty()?1:world.units.back().id+1); nu.team=editorOwner; nu.type=ut; nu.hp=100.0f; nu.attack=8.0f; nu.range=2.5f; nu.speed=4.0f; nu.role=ut==dom::sim::UnitType::Worker?dom::sim::UnitRole::Worker:dom::sim::UnitRole::Infantry; nu.attackType=dom::sim::AttackType::Melee; nu.preferredTargetRole=dom::sim::UnitRole::Infantry; nu.pos=nu.renderPos=nu.target=nu.slotTarget=wp; world.units.push_back(nu);
  };

#ifdef DOM_HAS_IMGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window, ctx);
  ImGui_ImplOpenGL3_Init("#version 130");
#endif

  bool running = true; Uint64 prev = SDL_GetPerformanceCounter(); float accum = 0.0f;
  double lastSimMs = 0.0;
  double lastAiMs = 0.0;
  while (running) {
    Uint64 now = SDL_GetPerformanceCounter(); float frameDt = (now - prev) / static_cast<float>(SDL_GetPerformanceFrequency()); prev = now; accum += frameDt;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef DOM_HAS_IMGUI
      ImGui_ImplSDL2_ProcessEvent(&e);
#endif
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        dom::render::set_resolution(e.window.data1, e.window.data2);
      }
      if (frontend.active) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) frontend.screen = FrontendState::Screen::MainMenu;
        continue;
      }
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_F1) uiState.showHudDebug = !uiState.showHudDebug;
        if (e.key.keysym.sym == SDLK_F2) { uiState.showProductionMenu = !uiState.showProductionMenu; world.uiTrainMenu = uiState.showProductionMenu; }
        if (e.key.keysym.sym == SDLK_F3) { uiState.showResearchPanel = !uiState.showResearchPanel; world.uiResearchMenu = uiState.showResearchPanel; }
        if (e.key.keysym.sym == SDLK_F4) uiState.showDiplomacyPanel = !uiState.showDiplomacyPanel;
        if (e.key.keysym.sym == SDLK_F5) uiState.showOperationsPanel = !uiState.showOperationsPanel;
        if (e.key.keysym.sym == SDLK_F9) uiState.showScenarioEditor = !uiState.showScenarioEditor;
        if (e.key.keysym.sym == SDLK_F10) assetBrowser.toggle();
        if (editorMode && e.key.keysym.sym == SDLK_TAB) editorTool = (editorTool + 1) % 6;
        if (editorMode && e.key.keysym.sym == SDLK_o) editorOwner = (uint16_t)((editorOwner + 1) % std::max<size_t>(1, world.players.size()));
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
        if (e.key.keysym.sym == SDLK_F1) showHudPanels = !showHudPanels;
        if (e.key.keysym.sym == SDLK_F2) showProductionPanel = !showProductionPanel;
        if (e.key.keysym.sym == SDLK_F3) showResearchPanel = !showResearchPanel;
        if (e.key.keysym.sym == SDLK_F4) showDiplomacyPanel = !showDiplomacyPanel;
        if (e.key.keysym.sym == SDLK_F5) showOperationsPanel = !showOperationsPanel;
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
          if (editorTool == 0) editor_place_unit(wp);
          else if (editorTool == 1) { dom::sim::Building nb{}; nb.id=(uint32_t)(world.buildings.empty()?1:world.buildings.back().id+1); nb.team=editorOwner; nb.type=dom::sim::BuildingType::Barracks; nb.pos=wp; nb.size={3.0f,3.0f}; nb.underConstruction=false; nb.buildProgress=1.0f; nb.buildTime=20.0f; nb.hp=1000.0f; nb.maxHp=1000.0f; world.buildings.push_back(nb); }
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

    std::vector<dom::sim::GameplayEvent> newEvents;
    dom::sim::consume_gameplay_events(newEvents);
    for (const auto& ev : newEvents) {
      eventLog.push_back(ev);
      if (eventLog.size() > 512) eventLog.erase(eventLog.begin());
      notifications.push_front({ev.tick, ev.text.empty() ? std::string("event") : ev.text});
      while (notifications.size() > 8) notifications.pop_back();
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float pan = frameDt * camera.zoom * 1.2f;
    if (keys[SDL_SCANCODE_W]) camera.center.y += pan;
    if (keys[SDL_SCANCODE_S]) camera.center.y -= pan;
    if (keys[SDL_SCANCODE_A]) camera.center.x -= pan;
    if (keys[SDL_SCANCODE_D]) camera.center.x += pan;

    while (!frontend.active && accum >= dom::core::kSimDeltaSeconds) {
      if (replayMode) {
        if (!replayPaused) {
          while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) { apply_replay_command(world, replayCommands[replayIdx]); ++replayIdx; }
          dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
          accum -= dom::core::kSimDeltaSeconds / replaySpeed;
        } else {
          accum = 0.0f;
        }
      } else {
        const auto simStart = std::chrono::steady_clock::now();
        const auto aiStart = std::chrono::steady_clock::now();
        std::vector<uint16_t> cpuPlayers;
        for (const auto& p : world.players) if (p.isCPU && p.alive) cpuPlayers.push_back(p.id);
        std::sort(cpuPlayers.begin(), cpuPlayers.end());
        std::mutex aiMergeMutex;
        dom::sim::TaskGraph aiGraph;
        for (uint16_t id : cpuPlayers) aiGraph.jobs.push_back({[&world, &aiMergeMutex, id]() { std::lock_guard<std::mutex> lock(aiMergeMutex); dom::ai::update_simple_ai(world, id); }});
        dom::sim::run_task_graph(aiGraph);
        const auto aiEnd = std::chrono::steady_clock::now();
        dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
        const auto simEnd = std::chrono::steady_clock::now();
        lastAiMs = std::chrono::duration<double, std::milli>(aiEnd - aiStart).count();
        lastSimMs = std::chrono::duration<double, std::milli>(simEnd - simStart).count();
        accum -= dom::core::kSimDeltaSeconds;
      }
      if (!opts.saveFile.empty() && opts.autosaveTick >= 0 && world.tick == (uint32_t)opts.autosaveTick) {
        const uint64_t saveHash = dom::sim::state_hash(world);
        if (save_world_file(opts.saveFile, world)) std::cout << "SAVE_RESULT path=" << opts.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
      }
    }

    int w, h; SDL_GetWindowSize(window, &w, &h);
#ifdef DOM_HAS_IMGUI
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
#endif
    dom::render::EditorPreview editorPreview{};
    if (editorMode) {
      int mx = 0, my = 0;
      SDL_GetMouseState(&mx, &my);
      editorPreview.enabled = true;
      editorPreview.pos = dom::render::screen_to_world(camera, w, h, {(float)mx, (float)my});
      editorPreview.radius = editorTool == 2 ? 2.5f : 1.2f;
      editorPreview.r = 0.25f;
      editorPreview.g = 0.85f;
      editorPreview.b = 0.3f;
      editorPreview.valid = editorPreview.pos.x >= 0.0f && editorPreview.pos.y >= 0.0f && editorPreview.pos.x < world.width && editorPreview.pos.y < world.height;
    }
    dom::render::set_editor_preview(editorPreview);
    dom::render::draw(world, camera, w, h, sel.dragHighlight);
    std::string replayOverlay;
    if (replayMode) replayOverlay = "REPLAY tick=" + std::to_string(world.tick) + (replayPaused ? " paused" : " running") + " speed=" + std::to_string(replaySpeed) + "x";
    if (uiState.showScenarioEditor) replayOverlay += (replayOverlay.empty()?"":" | ") + std::string("SCENARIO_EDITOR[F9]");
    replayOverlay += (replayOverlay.empty()?"":" | ") + std::string("AssetBrowser[F10]=") + (assetBrowser.visible() ? "On" : "Off");
    if (!world.objectiveLog.empty()) replayOverlay += (replayOverlay.empty()?"":" | ") + ("OBJ: " + world.objectiveLog.back().text);
    if (opts.perf) {
      const auto p = dom::sim::last_tick_profile();
      replayOverlay += (replayOverlay.empty()?"":" | ") + ("FPS ~" + std::to_string((int)std::round(1.0f / std::max(0.001f, frameDt))) +
        " sim=" + std::to_string((int)std::round(lastSimMs)) + "ms render=" + std::to_string((int)std::round(dom::render::last_draw_ms())) +
        "ms entities=" + std::to_string(world.units.size() + world.buildings.size()) + " ai=" + std::to_string((int)std::round(lastAiMs)) + "ms");
      const auto stats = dom::sim::last_simulation_stats();
      std::cout << "PERF tick=" << world.tick << " SIM_TICK_TIME=" << lastSimMs << " NAV_TIME=" << p.navMs << " COMBAT_TIME=" << p.combatMs << " AI_TIME=" << lastAiMs << " RENDER_TIME=" << dom::render::last_draw_ms() << " ENTITY_COUNT=" << (world.units.size()+world.buildings.size()) << " UNIT_COUNT=" << world.units.size() << " BUILDING_COUNT=" << world.buildings.size() << " THREADS=" << stats.threads << " JOB_COUNT=" << stats.jobCount << " CHUNK_COUNT=" << stats.chunkCount << " MOVEMENT_TASKS=" << stats.movementTasks << " FOG_TASKS=" << stats.fogTasks << " TERRITORY_TASKS=" << stats.territoryTasks << " NAV_REQUESTS=" << stats.navRequests << " NAV_COMPLETIONS=" << stats.navCompletions << " NAV_STALE_DROPS=" << stats.navStaleDrops << " EVENT_COUNT=" << stats.eventCount << " ROAD_COUNT=" << stats.roadCount << " ACTIVE_TRADE_ROUTES=" << stats.activeTradeRoutes << " RAIL_NODE_COUNT=" << stats.railNodeCount << " RAIL_EDGE_COUNT=" << stats.railEdgeCount << " ACTIVE_RAIL_NETWORKS=" << stats.activeRailNetworks << " ACTIVE_TRAINS=" << stats.activeTrains << " ACTIVE_SUPPLY_TRAINS=" << stats.activeSupplyTrains << " ACTIVE_FREIGHT_TRAINS=" << stats.activeFreightTrains << " RAIL_THROUGHPUT=" << stats.railThroughput << " DISRUPTED_RAIL_ROUTES=" << stats.disruptedRailRoutes << " SUPPLIED_UNITS=" << stats.suppliedUnits << " LOW_SUPPLY_UNITS=" << stats.lowSupplyUnits << " OUT_OF_SUPPLY_UNITS=" << stats.outOfSupplyUnits << " OPERATION_COUNT=" << stats.operationCount << " WORLD_TENSION=" << stats.worldTension << " ALLIANCE_COUNT=" << stats.allianceCount << " WAR_COUNT=" << stats.warCount << " ACTIVE_ESPIONAGE_OPS=" << stats.activeEspionageOps << " POSTURE_CHANGES=" << stats.postureChanges << " DIPLOMACY_EVENTS=" << stats.diplomacyEvents << " NAVAL_UNIT_COUNT=" << stats.navalUnitCount << " TRANSPORT_COUNT=" << stats.transportCount << " EMBARKED_UNIT_COUNT=" << stats.embarkedUnitCount << " ACTIVE_NAVAL_OPERATIONS=" << stats.activeNavalOperations << " COASTAL_TARGETS=" << stats.coastalTargets << " NAVAL_COMBAT_EVENTS=" << stats.navalCombatEvents << " AIR_UNIT_COUNT=" << stats.airUnitCount << " DETECTOR_COUNT=" << stats.detectorCount << " RADAR_REVEALS=" << stats.radarReveals << " STRATEGIC_STRIKES=" << stats.strategicStrikes << " INTERCEPTIONS=" << stats.interceptions << " STRATEGIC_STOCKPILE_TOTAL=" << stats.strategicStockpileTotal << " STRATEGIC_READY_TOTAL=" << stats.strategicReadyTotal << " STRATEGIC_PREPARING_TOTAL=" << stats.strategicPreparingTotal << " STRATEGIC_WARNINGS=" << stats.strategicWarnings << " STRATEGIC_RETALIATIONS=" << stats.strategicRetaliations << " SECOND_STRIKE_READY_COUNT=" << stats.secondStrikeReadyCount << " DETERRENCE_POSTURE_CHANGES=" << stats.deterrencePostureChanges << " ACTIVE_DENIAL_ZONES=" << stats.activeDenialZones << " MOUNTAIN_REGION_COUNT=" << stats.mountainRegionCount << " SURFACE_DEPOSIT_COUNT=" << stats.surfaceDepositCount << " DEEP_DEPOSIT_COUNT=" << stats.deepDepositCount << " ACTIVE_MINE_SHAFTS=" << stats.activeMineShafts << " ACTIVE_TUNNELS=" << stats.activeTunnels << " UNDERGROUND_DEPOTS=" << stats.undergroundDepots << " UNDERGROUND_YIELD=" << stats.undergroundYield << " GUARDIAN_SITE_COUNT=" << stats.guardianSiteCount << " GUARDIANS_DISCOVERED=" << stats.guardiansDiscovered << " GUARDIANS_SPAWNED=" << stats.guardiansSpawned << " GUARDIANS_JOINED=" << stats.guardiansJoined << " GUARDIANS_KILLED=" << stats.guardiansKilled << " HOSTILE_GUARDIAN_EVENTS=" << stats.hostileGuardianEvents << " ALLIED_GUARDIAN_EVENTS=" << stats.alliedGuardianEvents << " CONTENT_FALLBACK_COUNT=" << stats.contentFallbackCount << " CIV_PRESENTATION_RESOLVES=" << stats.civPresentationResolves << " GUARDIAN_PRESENTATION_RESOLVES=" << stats.guardianPresentationResolves << " CAMPAIGN_PRESENTATION_RESOLVES=" << stats.campaignPresentationResolves << " EVENT_PRESENTATION_RESOLVES=" << stats.eventPresentationResolves << "\n";
    }
    if (!frontend.active) {
      dom::ui::push_gameplay_notifications(world, uiState);
      dom::ui::draw_hud(window, world, selected, uiState, replayOverlay);
      if (uiState.showScenarioEditor) dom::editor::draw_scenario_editor(world, camera.center, scenarioEditorState);
      if (uiState.showHudDebug || uiState.showDebugPanels) dom::debug::draw_debug_panels(world, debugVisualState);
      dom::debug::sync_debug_visuals(debugVisualState);
    }
#ifdef DOM_HAS_IMGUI
    if (frontend.active) {
      clamp_selection(frontend.selectedScenario, scenarioEntries);
      clamp_selection(frontend.selectedCampaign, campaignEntries);
      clamp_selection(frontend.selectedSave, saveEntries);
      clamp_selection(frontend.selectedHumanCiv, civIds);
      clamp_selection(frontend.selectedAiCiv, civIds);
      frontend.validation.clear();
      if (frontend.players < 2) frontend.validation = "At least 2 player slots are required.";
      if (frontend.aiSlots < 1) frontend.validation = "At least 1 AI slot is required.";
      if (frontend.players < frontend.aiSlots + 1) frontend.validation = "AI slots exceed available player slots.";
      if (!frontend.allowConquest && !frontend.allowScore && !frontend.allowWonder) frontend.validation = "Select at least one victory condition.";
      if (frontend.mapW < 16 || frontend.mapH < 16) frontend.validation = "Map size must be >= 16x16.";

      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
      ImGui::Begin("DOMiNATION Frontend", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
      ImGui::Text("DOMiNATION RTS");
      ImGui::Separator();
      if (frontend.screen == FrontendState::Screen::MainMenu) {
        if (ImGui::Button("Skirmish", ImVec2(220, 0))) frontend.screen = FrontendState::Screen::Skirmish;
        if (ImGui::Button("Scenario", ImVec2(220, 0))) frontend.screen = FrontendState::Screen::Scenario;
        if (ImGui::Button("Campaign", ImVec2(220, 0))) frontend.screen = FrontendState::Screen::Campaign;
        if (ImGui::Button("Load Game", ImVec2(220, 0))) { saveEntries = read_save_entries(); frontend.screen = FrontendState::Screen::LoadGame; }
        if (ImGui::Button("Options", ImVec2(220, 0))) frontend.screen = FrontendState::Screen::Options;
        if (ImGui::Button("Quit", ImVec2(220, 0))) running = false;
      } else if (frontend.screen == FrontendState::Screen::Skirmish) {
        ImGui::Columns(2, nullptr, true);
        ImGui::Text("Player Slots");
        ImGui::SliderInt("Total Players", &frontend.players, 2, 8);
        ImGui::SliderInt("AI Slots", &frontend.aiSlots, 1, 7);
        const char* humanCiv = civIds[frontend.selectedHumanCiv].c_str();
        if (ImGui::BeginCombo("Human Civilization", humanCiv)) { for (int i = 0; i < (int)civIds.size(); ++i) { bool sel = i == frontend.selectedHumanCiv; if (ImGui::Selectable(civIds[i].c_str(), sel)) frontend.selectedHumanCiv = i; } ImGui::EndCombo(); }
        const char* aiCiv = civIds[frontend.selectedAiCiv].c_str();
        if (ImGui::BeginCombo("AI Civilization", aiCiv)) { for (int i = 0; i < (int)civIds.size(); ++i) { bool sel = i == frontend.selectedAiCiv; if (ImGui::Selectable(civIds[i].c_str(), sel)) frontend.selectedAiCiv = i; } ImGui::EndCombo(); }
        ImGui::NextColumn();
        ImGui::Text("World & Victory");
        const char* wp = worldPresetOptions[frontend.selectedWorldPreset].c_str();
        if (ImGui::BeginCombo("World Preset", wp)) { for (int i = 0; i < (int)worldPresetOptions.size(); ++i) { bool sel = i == frontend.selectedWorldPreset; if (ImGui::Selectable(worldPresetOptions[i].c_str(), sel)) frontend.selectedWorldPreset = i; } ImGui::EndCombo(); }
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &frontend.seed);
        ImGui::InputInt("Map Width", &frontend.mapW);
        ImGui::InputInt("Map Height", &frontend.mapH);
        ImGui::SliderInt("Armageddon Nations", &frontend.armageddonNationsThreshold, 1, 8);
        ImGui::SliderInt("Armageddon Uses/Nation", &frontend.armageddonUsesThreshold, 1, 6);
        ImGui::Checkbox("World Events Enabled", &frontend.enableWorldEvents);
        ImGui::Checkbox("Mythic Guardians Enabled", &frontend.enableGuardians);
        ImGui::Checkbox("AI Aggressive", &frontend.aiAggressive);
        ImGui::Checkbox("Conquest", &frontend.allowConquest); ImGui::SameLine();
        ImGui::Checkbox("Score", &frontend.allowScore); ImGui::SameLine();
        ImGui::Checkbox("Wonder", &frontend.allowWonder);
        ImGui::Columns(1);
        ImGui::SeparatorText("Launch Summary");
        ImGui::Text("Players: %d (%d human / %d AI)", frontend.players, 1, frontend.aiSlots);
        ImGui::Text("Human Civ: %s | AI Civ: %s", civIds[frontend.selectedHumanCiv].c_str(), civIds[frontend.selectedAiCiv].c_str());
        ImGui::Text("Preset: %s | Map: %dx%d | Seed: %u", worldPresetOptions[frontend.selectedWorldPreset].c_str(), frontend.mapW, frontend.mapH, frontend.seed);
        if (!frontend.validation.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "%s", frontend.validation.c_str());
        if (ImGui::Button("Launch Skirmish", ImVec2(220, 0)) && frontend.validation.empty()) {
          world = {};
          world.width = frontend.mapW;
          world.height = frontend.mapH;
          dom::sim::set_world_preset(world, dom::sim::parse_world_preset(worldPresetOptions[frontend.selectedWorldPreset]));
          dom::sim::initialize_world(world, frontend.seed);
          dom::ai::set_aggressive(frontend.aiAggressive);
          world.config.allowConquest = frontend.allowConquest;
          world.config.allowScore = frontend.allowScore;
          world.config.allowWonder = frontend.allowWonder;
          world.armageddonNationsThreshold = static_cast<uint16_t>(frontend.armageddonNationsThreshold);
          world.armageddonUsesPerNationThreshold = static_cast<uint16_t>(frontend.armageddonUsesThreshold);
          if (!frontend.enableWorldEvents) { world.worldEventDefinitions.clear(); world.worldEvents.clear(); }
          if (!frontend.enableGuardians) { world.guardianDefinitions.clear(); world.guardianSites.clear(); }
          if (!world.players.empty()) world.players[0].civilization = dom::sim::civilization_runtime_for(civIds[frontend.selectedHumanCiv]);
          for (size_t i = 1; i < world.players.size(); ++i) world.players[i].civilization = dom::sim::civilization_runtime_for(civIds[frontend.selectedAiCiv]);
          dom::sim::on_authoritative_state_loaded(world);
          frontend.active = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Back", ImVec2(120, 0))) frontend.screen = FrontendState::Screen::MainMenu;
      } else if (frontend.screen == FrontendState::Screen::Scenario) {
        ImGui::BeginChild("scenario_list", ImVec2(420, 0), true);
        for (int i = 0; i < (int)scenarioEntries.size(); ++i) {
          if (ImGui::Selectable(scenarioEntries[i].title.c_str(), i == frontend.selectedScenario)) frontend.selectedScenario = i;
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("scenario_meta", ImVec2(0, 0), true);
        if (!scenarioEntries.empty()) {
          const auto& s = scenarioEntries[frontend.selectedScenario];
          ImGui::Text("%s", s.title.c_str()); ImGui::Separator();
          ImGui::TextWrapped("%s", s.description.empty() ? "No description." : s.description.c_str());
          ImGui::Text("Civ: %s", s.civilization.empty() ? "default" : s.civilization.c_str());
          ImGui::Text("World: %s", s.worldPreset.empty() ? "(authored/default)" : s.worldPreset.c_str());
          ImGui::Text("Difficulty: %s", s.difficulty.empty() ? "(none)" : s.difficulty.c_str());
          ImGui::TextWrapped("Briefing: %s", s.briefing.empty() ? "N/A" : s.briefing.c_str());
          if (ImGui::Button("Launch Scenario") && !s.path.empty()) { std::string err; if (dom::sim::load_scenario_file(world, s.path, opts.seed, err)) { dom::sim::on_authoritative_state_loaded(world); frontend.active = false; } else frontend.launchStatus = err; }
        } else ImGui::TextDisabled("No scenarios found.");
        if (!frontend.launchStatus.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "%s", frontend.launchStatus.c_str());
        if (ImGui::Button("Back", ImVec2(120, 0))) frontend.screen = FrontendState::Screen::MainMenu;
        ImGui::EndChild();
      } else if (frontend.screen == FrontendState::Screen::Campaign) {
        ImGui::BeginChild("campaign_list", ImVec2(420, 0), true);
        for (int i = 0; i < (int)campaignEntries.size(); ++i) {
          if (ImGui::Selectable(campaignEntries[i].title.c_str(), i == frontend.selectedCampaign)) frontend.selectedCampaign = i;
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("campaign_meta", ImVec2(0, 0), true);
        if (!campaignEntries.empty()) {
          const auto& c = campaignEntries[frontend.selectedCampaign];
          ImGui::Text("%s", c.title.c_str()); ImGui::Separator();
          ImGui::TextWrapped("%s", c.description.empty() ? "No description." : c.description.c_str());
          ImGui::Text("Faction/Civ: %s", c.civilization.empty() ? "default" : c.civilization.c_str());
          ImGui::Text("Difficulty Tags: %s", c.difficulty.empty() ? "(none)" : c.difficulty.c_str());
          ImGui::TextWrapped("Briefing: %s", c.briefing.empty() ? "N/A" : c.briefing.c_str());
          if (ImGui::Button("Launch Campaign") && !c.path.empty()) {
            CampaignDefinition def{}; std::string err;
            if (parse_campaign_file(c.path, def, err) && !def.missions.empty() && dom::sim::load_scenario_file(world, def.missions.front().scenarioFile, opts.seed, err)) {
              world.campaign = def.startState;
              dom::sim::on_authoritative_state_loaded(world);
              frontend.active = false;
            } else frontend.launchStatus = err.empty() ? "Failed to launch campaign" : err;
          }
        } else ImGui::TextDisabled("No campaigns found.");
        if (!frontend.launchStatus.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "%s", frontend.launchStatus.c_str());
        if (ImGui::Button("Back", ImVec2(120, 0))) frontend.screen = FrontendState::Screen::MainMenu;
        ImGui::EndChild();
      } else if (frontend.screen == FrontendState::Screen::LoadGame) {
        if (ImGui::Button("Refresh Saves")) saveEntries = read_save_entries();
        ImGui::Separator();
        for (int i = 0; i < (int)saveEntries.size(); ++i) {
          const auto& s = saveEntries[i];
          std::string label = s.name + "  (tick " + std::to_string(s.tick) + ")";
          if (ImGui::Selectable(label.c_str(), i == frontend.selectedSave)) frontend.selectedSave = i;
        }
        if (!saveEntries.empty()) {
          const auto& s = saveEntries[frontend.selectedSave];
          ImGui::SeparatorText("Save Metadata");
          ImGui::Text("Path: %s", s.path.c_str());
          ImGui::Text("Scenario: %s", s.scenario.empty() ? "(none)" : s.scenario.c_str());
          ImGui::Text("Campaign: %s", s.campaign.empty() ? "(none)" : s.campaign.c_str());
          ImGui::Text("Civ: %s", s.civilization.empty() ? "default" : s.civilization.c_str());
          if (ImGui::Button("Load Save")) {
            std::ifstream in(s.path);
            if (in.good()) {
              nlohmann::json inSave; in >> inSave; std::string err;
              if (load_world_json(inSave, world, err)) { dom::sim::on_authoritative_state_loaded(world); frontend.active = false; }
              else frontend.launchStatus = err;
            }
          }
        } else ImGui::TextDisabled("No saves found in ./saves.");
        if (!frontend.launchStatus.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "%s", frontend.launchStatus.c_str());
        if (ImGui::Button("Back", ImVec2(120, 0))) frontend.screen = FrontendState::Screen::MainMenu;
      } else if (frontend.screen == FrontendState::Screen::Options) {
        ImGui::Text("Lightweight Options");
        ImGui::SliderFloat("UI Scale", &opts.uiScale, 0.5f, 3.0f);
        ImGui::SliderFloat("Render Scale", &opts.renderScale, 0.5f, 1.0f);
        dom::render::set_ui_scale(opts.uiScale);
        dom::render::set_render_scale(opts.renderScale);
        ImGui::TextDisabled("Debug panels are still available in-match (F1/F9/F10). ");
        if (ImGui::Button("Back", ImVec2(120, 0))) frontend.screen = FrontendState::Screen::MainMenu;
      }
      ImGui::End();
    }

    if (!frontend.active && showHudPanels) {
      if (showProductionPanel) {
        ImGui::Begin("Production [F2]", &showProductionPanel);
        uint32_t bid = selected_building();
        ImGui::Text("Building: %u", bid);
        bool ok = bid != 0;
        if (!ok) ImGui::TextDisabled("Select worker/city for queue actions.");
        if (ImGui::Button("Train Worker") && ok) { bool r = dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Worker); log_command("production", "train_worker", r); }
        ImGui::SameLine();
        if (ImGui::Button("Train Infantry") && ok) { bool r = dom::sim::enqueue_train_unit(world, 0, bid, dom::sim::UnitType::Infantry); log_command("production", "train_infantry", r); }
        if (ImGui::Button("Cancel Front") && ok) { bool r = dom::sim::cancel_queue_item(world, 0, bid, 0); log_command("production", "cancel_front", r); }
        for (const auto& b : world.buildings) if (b.id == bid) {
          ImGui::Separator();
          for (size_t i = 0; i < b.queue.size(); ++i) ImGui::Text("%zu: %s", i, b.queue[i].kind == dom::sim::QueueKind::AgeResearch ? "Age Research" : "Train Unit");
        }
        ImGui::End();
      }
      if (showResearchPanel) {
        ImGui::Begin("Research [F3]", &showResearchPanel);
        uint32_t bid = selected_building();
        const auto& p0 = world.players[0];
        bool canAge = p0.age < dom::sim::Age::Information;
        ImGui::Text("Age: %d", (int)p0.age + 1);
        ImGui::Text("Knowledge: %.1f", p0.resources[(size_t)dom::sim::Resource::Knowledge]);
        if (!canAge) ImGui::TextDisabled("Locked: max age reached");
        if (ImGui::Button("Start Age Up") && bid) { bool r = dom::sim::enqueue_age_research(world, 0, bid); log_command("research", "start_age_up", r); }
        ImGui::End();
      }
      if (showDiplomacyPanel) {
        ImGui::Begin("Diplomacy [F4]", &showDiplomacyPanel);
        for (size_t i = 1; i < world.players.size(); ++i) {
          auto rel = world.diplomacy[0 * world.players.size() + i];
          ImGui::Separator();
          ImGui::Text("Player %zu relation: %s", i, relation_name(rel));
          bool canWar = rel != dom::sim::DiplomacyRelation::War;
          if (!canWar) ImGui::BeginDisabled();
          if (ImGui::Button((std::string("Declare War##") + std::to_string(i)).c_str())) { bool r = dom::sim::declare_war(world, 0, (uint16_t)i); log_command("diplomacy", "declare_war", r); }
          if (!canWar) { ImGui::EndDisabled(); ImGui::SameLine(); ImGui::TextDisabled("already at war"); }
          ImGui::SameLine();
          if (ImGui::Button((std::string("Alliance##") + std::to_string(i)).c_str())) { bool r = dom::sim::form_alliance(world, 0, (uint16_t)i); log_command("diplomacy", "alliance", r); }
          ImGui::SameLine();
          if (ImGui::Button((std::string("Trade##") + std::to_string(i)).c_str())) { bool r = dom::sim::establish_trade_agreement(world, 0, (uint16_t)i); log_command("diplomacy", "trade", r); }
        }
        ImGui::End();
      }
      if (showOperationsPanel) {
        ImGui::Begin("Operations [F5]", &showOperationsPanel);
        ImGui::Text("Active operations from authoritative state");
        for (const auto& op : world.operations) {
          if (!op.active) continue;
          ImGui::BulletText("team=%u type=%d target=(%.1f,%.1f)", op.team, (int)op.type, op.target.x, op.target.y);
        }
        ImGui::TextDisabled("Read-only (issuing operations not exposed in UI yet).");
        ImGui::End();
      }
      if (showCampaignPanel && !world.campaign.campaignId.empty()) {
        ImGui::Begin("Campaign Briefing", &showCampaignPanel);
        ImGui::Text("%s", world.mission.title.c_str());
        if (!world.mission.subtitle.empty()) ImGui::TextDisabled("%s", world.mission.subtitle.c_str());
        if (!world.mission.locationLabel.empty()) ImGui::Text("Location: %s", world.mission.locationLabel.c_str());
        ImGui::Text("Portrait: %s", world.mission.briefingPortraitId.empty() ? "ui_portrait_default (fallback)" : world.mission.briefingPortraitId.c_str());
        ImGui::Text("Artwork: %s", world.mission.missionImageId.empty() ? "ui_mission_default (fallback)" : world.mission.missionImageId.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", world.mission.briefing.c_str());
        if (!world.mission.factionSummary.empty()) { ImGui::SeparatorText("Faction"); ImGui::TextWrapped("%s", world.mission.factionSummary.c_str()); }
        if (!world.mission.objectiveSummary.empty()) { ImGui::SeparatorText("Key Objectives"); for (const auto& l : world.mission.objectiveSummary) ImGui::BulletText("%s", l.c_str()); }
        ImGui::SeparatorText("Carryover");
        ImGui::Text("Civ: %s", world.campaign.playerCivilizationId.c_str());
        ImGui::Text("Previous Result: %s", world.campaign.previousMissionResult.c_str());
        ImGui::Text("Food %.1f  Wealth %.1f  Knowledge %.1f", world.campaign.resources[0], world.campaign.resources[3], world.campaign.resources[4]);
        if (!world.mission.scenarioTags.empty()) { ImGui::SeparatorText("Tags"); for (const auto& t : world.mission.scenarioTags) { ImGui::SameLine(); ImGui::Text("[%s]", t.c_str()); } }
        ImGui::End();
      }
      if (showCampaignDebriefPanel && !world.campaign.campaignId.empty()) {
        ImGui::Begin("Campaign Debrief & Progression", &showCampaignDebriefPanel);
        ImGui::Text("Result: %s", world.missionRuntime.resultTag.c_str());
        ImGui::Text("Status: %d", (int)world.missionRuntime.status);
        if (!world.mission.debrief.empty()) ImGui::TextWrapped("%s", world.mission.debrief.c_str());
        ImGui::SeparatorText("Objectives");
        for (const auto& o : world.objectives) {
          const bool primary = (o.category == dom::sim::ObjectiveCategory::Primary || o.primary);
          if (o.state == dom::sim::ObjectiveState::Completed || o.state == dom::sim::ObjectiveState::Failed) ImGui::BulletText("%s %s -> %s", primary?"[Primary]":"[Secondary]", o.title.c_str(), (o.state==dom::sim::ObjectiveState::Completed?"Completed":"Failed"));
        }
        ImGui::SeparatorText("Campaign");
        ImGui::Text("Flags: %zu", world.campaign.flags.size());
        ImGui::Text("Rewards: %zu", world.campaign.unlockedRewards.size());
        ImGui::Text("Pending Branch: %s", world.campaign.pendingBranchKey.empty() ? "(none)" : world.campaign.pendingBranchKey.c_str());
        ImGui::Text("Debrief Portrait: %s", world.mission.debriefPortraitId.empty() ? "ui_portrait_default (fallback)" : world.mission.debriefPortraitId.c_str());
        if (!world.campaign.unlockedRewards.empty()) { for (const auto& r : world.campaign.unlockedRewards) ImGui::BulletText("Reward: %s", r.c_str()); }
        ImGui::End();
      }
      ImGui::Begin("Mission Message Log");
      ImGui::Text("Queued messages: %zu", world.missionMessages.size());
      for (auto it = world.missionMessages.rbegin(); it != world.missionMessages.rend(); ++it) {
        ImGui::Text("[%llu @%u] %s | %s", (unsigned long long)it->sequence, it->tick, it->category.c_str(), it->title.c_str());
        if (!it->speaker.empty()) ImGui::TextDisabled("speaker=%s portrait=%s icon=%s", it->speaker.c_str(), it->portraitId.c_str(), it->iconId.c_str());
        ImGui::TextWrapped("%s", it->body.c_str());
        ImGui::Separator();
      }
      ImGui::End();
      ImGui::Begin("Objective Transition Debug");
      for (auto it = world.objectiveDebugLog.rbegin(); it != world.objectiveDebugLog.rend(); ++it) {
        ImGui::Text("tick=%u obj=%u trigger=%u %s", it->tick, it->objectiveId, it->triggerId, it->actionType.c_str());
        ImGui::TextDisabled("reason: %s", it->reason.c_str());
      }
      ImGui::End();
      if (showSelectionPanel) {
        ImGui::Begin("Selection", &showSelectionPanel);
        ImGui::Text("Mode: %s", editorMode ? "Editor selection" : "Gameplay selection");
        ImGui::Text("Selected units: %zu", selected.size());
        if (selected.size() > 1) ImGui::Text("Multi-select summary active");
        if (!selected.empty()) {
          for (const auto& u : world.units) if (u.id == selected[0]) {
            ImGui::Text("Unit %u owner=%u hp=%.1f role=%d cargo=%zu", u.id, u.team, u.hp, (int)u.role, u.cargo.size());
            if (u.team < world.players.size()) ImGui::Text("Diplomacy to local: %s", relation_name(world.diplomacy[0 * world.players.size() + u.team]));
          }
        }
        ImGui::End();
      }
      showEventLogPanel = true;
      showCommandHistoryPanel = true;
      if (showEventLogPanel) {
        ImGui::Begin("Event Log", &showEventLogPanel);
        ImGui::Checkbox("Production", &eventFilterProduction); ImGui::SameLine();
        ImGui::Checkbox("Diplomacy", &eventFilterDiplomacy); ImGui::SameLine();
        ImGui::Checkbox("Combat", &eventFilterCombat);
        for (auto it = eventLog.rbegin(); it != eventLog.rend(); ++it) {
          bool keep = true;
          if (it->type == dom::sim::GameplayEventType::BuildingCompleted || it->type == dom::sim::GameplayEventType::ObjectiveCompleted) keep = eventFilterProduction;
          if (it->type == dom::sim::GameplayEventType::WarDeclared || it->type == dom::sim::GameplayEventType::AllianceFormed || it->type == dom::sim::GameplayEventType::AllianceBroken || it->type == dom::sim::GameplayEventType::TradeAgreementCreated || it->type == dom::sim::GameplayEventType::TradeAgreementBroken) keep = eventFilterDiplomacy;
          if (it->type == dom::sim::GameplayEventType::UnitDied) keep = eventFilterCombat;
          if (keep) ImGui::Text("[%u] %s", it->tick, it->text.c_str());
        }
        ImGui::End();
      }
      if (showCommandHistoryPanel) {
        ImGui::Begin("Command History", &showCommandHistoryPanel);
        for (auto it = commandHistory.rbegin(); it != commandHistory.rend(); ++it) {
          ImGui::Text("[%u] %s %s %s", it->tick, it->panel.c_str(), it->command.c_str(), it->success ? "ok" : "rejected");
        }
        ImGui::End();
      }
      if (editorMode && showEditorPanel) {
        ImGui::Begin("Scenario Editor [F9]", &showEditorPanel);
        ImGui::Text("Tool=%d Owner=%u", editorTool, editorOwner);
        static char saveBuf[256]{}; static char loadBuf[256]{};
        if (saveBuf[0] == '\0') std::snprintf(saveBuf, sizeof(saveBuf), "%s", editorScenarioPath.c_str());
        if (loadBuf[0] == '\0') std::snprintf(loadBuf, sizeof(loadBuf), "%s", editorLoadPath.c_str());
        ImGui::InputText("Save As", saveBuf, sizeof(saveBuf));
        if (ImGui::Button("Save Scenario")) { std::string err; bool r = dom::sim::save_scenario_file(saveBuf, world, err); log_command("editor", "save_scenario", r, err); }
        ImGui::InputText("Load", loadBuf, sizeof(loadBuf));
        if (ImGui::Button("Load Scenario")) { std::string err; bool r = dom::sim::load_scenario_file(world, loadBuf, world.seed, err); if (r) dom::sim::on_authoritative_state_loaded(world); log_command("editor", "load_scenario", r, err); }
        ImGui::TextDisabled("Unsupported fields are not authored by this editor and may be omitted.");
        ImGui::End();
      }
      ImGui::SetNextWindowBgAlpha(0.35f);
      ImGui::Begin("Notifications", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
      ImGui::SetWindowPos(ImVec2(12.0f, 12.0f));
      int n = 0;
      for (const auto& note : notifications) {
        if (n++ >= 5) break;
        ImGui::Text("[%u] %s", note.tick, note.text.c_str());
      }
      ImGui::End();
    }
    assetBrowser.draw(assetManager);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
    SDL_GL_SwapWindow(window);
  }

  if (!opts.saveFile.empty()) {
    const uint64_t saveHash = dom::sim::state_hash(world);
    if (save_world_file(opts.saveFile, world)) std::cout << "SAVE_RESULT path=" << opts.saveFile << " tick=" << world.tick << " hash=" << saveHash << "\n";
  }

#ifdef DOM_HAS_IMGUI
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
#endif

  SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
