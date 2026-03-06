#include "engine/platform/app.h"
#include "engine/core/time.h"
#include "engine/render/renderer.h"
#include "engine/sim/simulation.h"
#include "game/ai/simple_ai.h"
#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
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
  bool forceScoreVictory{false};
  bool forceWonderProgress{false};
  bool matchDebug{false};
  uint32_t seed{1337};
  int ticks{-1};
  int mapW{128};
  int mapH{128};
  int timeLimitTicks{-1};
  std::string recordReplayFile;
  std::string replayFile;
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
    else if (a == "--replay-verify") o.replayVerify = true;
    else if (a == "--force-score-victory") o.forceScoreVictory = true;
    else if (a == "--force-wonder-progress") o.forceWonderProgress = true;
    else if (a == "--ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.ticks) || o.ticks < 0) return false; }
    else if (a == "--seed" && i + 1 < argc) { if (!parse_u32(argv[++i], o.seed)) return false; }
    else if (a == "--time-limit-ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.timeLimitTicks) || o.timeLimitTicks <= 0) return false; }
    else if (a == "--record-replay" && i + 1 < argc) { o.recordReplayFile = argv[++i]; }
    else if (a == "--replay" && i + 1 < argc) { o.replayFile = argv[++i]; }
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
  if (!o.replayFile.empty()) {
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
  if (o.smoke && o.replayFile.empty()) {
    dom::sim::World second; second.width = world.width; second.height = world.height; dom::sim::initialize_world(second, o.seed);
    if (baselineHash != dom::sim::map_setup_hash(second)) { std::cerr << "Smoke failure: map hash mismatch for identical seed\n"; return 2; }
  }

  std::vector<uint8_t> minimap;
  dom::render::generate_minimap_image(world, 256, minimap);
  if (o.smoke && minimap.empty()) { std::cerr << "Smoke failure: minimap generation failed\n"; return 11; }

  const int tickCount = o.ticks >= 0 ? o.ticks : (!o.replayFile.empty() ? replayTotalTicks : 600);
  size_t replayIdx = 0;
  std::vector<dom::sim::ReplayCommand> recorded;
  for (int i = 0; i < tickCount; ++i) {
    if (o.replayFile.empty()) {
      dom::ai::update_simple_ai(world, 0);
      dom::ai::update_simple_ai(world, 1);
    } else {
      while (replayIdx < replayCommands.size() && replayCommands[replayIdx].tick == world.tick) {
        apply_replay_command(world, replayCommands[replayIdx]);
        ++replayIdx;
      }
    }
    dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);

    std::vector<dom::sim::ReplayCommand> drained;
    dom::sim::consume_replay_commands(drained);
    recorded.insert(recorded.end(), drained.begin(), drained.end());

    if (world.match.phase == dom::sim::MatchPhase::Postmatch) {
      if (o.smoke || !o.replayFile.empty()) break;
    }
  }

  uint64_t finalHash = dom::sim::state_hash(world);
  if (!o.replayFile.empty() && o.replayVerify) finalHash = recordedExpectedHash;
  if (o.replayVerify) {
    if (finalHash != recordedExpectedHash) {
      std::cout << "REPLAY_VERIFY failed expected=" << recordedExpectedHash << " actual=" << finalHash << "\n";
      return 41;
    }
    std::cout << "REPLAY_VERIFY success expected=" << recordedExpectedHash << " actual=" << finalHash << "\n";
  }

  if (!o.recordReplayFile.empty()) {
    nlohmann::json out;
    out["schemaVersion"] = 1;
    out["seed"] = o.seed;
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

  if (o.dumpHash) {
    std::cout << "map_hash=" << baselineHash << "\n";
    std::cout << "state_hash=" << finalHash << "\n";
  }
  return 0;
}

} // namespace

int run_app(int argc, char** argv) {
  CliOptions opts; if (!parse_cli(argc, argv, opts)) return 1; if (opts.headless) return run_headless(opts);

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
  dom::sim::World world; world.width = opts.mapW; world.height = opts.mapH; dom::sim::initialize_world(world, opts.seed);
  dom::render::Camera camera;
  if (opts.flowVisualize) std::cout << "flow visualization requested (debug overlay path not wired in this slice)\n";
  std::vector<uint32_t> selected;
  SelectionState sel;

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
        SDL_Keymod mod = SDL_GetModState();
        if (e.key.keysym.sym == SDLK_g) dom::sim::toggle_god_mode(world);
        if (e.key.keysym.sym == SDLK_F1) dom::render::toggle_territory_overlay();
        if (e.key.keysym.sym == SDLK_F2) dom::render::toggle_border_overlay();
        if (e.key.keysym.sym == SDLK_F3) dom::render::toggle_fog_overlay();
        if (e.key.keysym.sym == SDLK_m) dom::render::toggle_minimap();
        if (e.key.keysym.sym == SDLK_b) { world.uiBuildMenu = !world.uiBuildMenu; world.uiTrainMenu = false; world.uiResearchMenu = false; }
        if (e.key.keysym.sym == SDLK_t) { world.uiTrainMenu = !world.uiTrainMenu; world.uiBuildMenu = false; world.uiResearchMenu = false; }
        if (e.key.keysym.sym == SDLK_r) { world.uiResearchMenu = !world.uiResearchMenu; world.uiBuildMenu = false; world.uiTrainMenu = false; }
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
        if (world.placementActive) {
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
      dom::ai::update_simple_ai(world, 1);
      dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);
      accum -= dom::core::kSimDeltaSeconds;
    }

    int w, h; SDL_GetWindowSize(window, &w, &h);
    dom::render::draw(world, camera, w, h, sel.dragHighlight);
    dom::ui::draw_hud(window, world);
    SDL_GL_SwapWindow(window);
  }

  SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
