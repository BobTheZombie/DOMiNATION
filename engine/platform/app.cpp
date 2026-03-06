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
  uint32_t seed{1337};
  int ticks{-1};
  int mapW{128};
  int mapH{128};
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
    else if (a == "--ticks" && i + 1 < argc) { if (!parse_int(argv[++i], o.ticks) || o.ticks < 0) return false; }
    else if (a == "--seed" && i + 1 < argc) { if (!parse_u32(argv[++i], o.seed)) return false; }
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

int run_headless(const CliOptions& o) {
  dom::sim::set_nav_debug(o.navDebug);
  dom::ai::set_attack_early(o.aiAttackEarly);
  dom::ai::set_aggressive(o.aiAggressive);
  dom::sim::set_combat_debug(o.combatDebug);
  dom::sim::World world; world.width = o.mapW; world.height = o.mapH; dom::sim::initialize_world(world, o.seed);
  const uint64_t baselineHash = dom::sim::map_setup_hash(world);

  dom::sim::World second; second.width = o.mapW; second.height = o.mapH; dom::sim::initialize_world(second, o.seed);
  if (o.smoke && baselineHash != dom::sim::map_setup_hash(second)) { std::cerr << "Smoke failure: map hash mismatch for identical seed\n"; return 2; }

  std::vector<uint8_t> minimap;
  dom::render::generate_minimap_image(world, 256, minimap);
  if (o.smoke && minimap.empty()) { std::cerr << "Smoke failure: minimap generation failed\n"; return 11; }

  const uint64_t preControlHash = dom::sim::state_hash(world);
  SelectionState s;
  for (const auto& u : world.units) if (u.team == 0 && s.controlGroups[0].size() < 4) s.controlGroups[0].push_back(u.id);
  auto snapshot = s.controlGroups[0];
  s.controlGroups[0] = snapshot;
  if (o.smoke && dom::sim::state_hash(world) != preControlHash) { std::cerr << "Smoke failure: control groups changed sim state\n"; return 12; }

  const int tickCount = o.ticks >= 0 ? o.ticks : 600;
  for (int i = 0; i < tickCount; ++i) {
    dom::ai::update_simple_ai(world, 0);
    dom::ai::update_simple_ai(world, 1);
    if (o.smoke && world.researchStartedCount == 0 && (i % 100) == 0) {
      uint32_t cc = first_building(world, 0, dom::sim::BuildingType::CityCenter);
      if (cc) dom::sim::enqueue_age_research(world, 0, cc);
    }
    dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);

    if (!o.smoke) continue;
    for (const auto& u : world.units) {
      if (u.id == 0 || !std::isfinite(u.hp) || !std::isfinite(u.pos.x) || !std::isfinite(u.pos.y)) { std::cerr << "Smoke failure: invalid unit state at tick " << i << "\n"; return 3; }
      if (u.pos.x < -2.0f || u.pos.y < -2.0f || u.pos.x > world.width + 2.0f || u.pos.y > world.height + 2.0f) { std::cerr << "Smoke failure: out-of-bounds unit at tick " << i << "\n"; return 4; }
    }
    if (world.gameOver && world.tick < 20 * 600) { std::cerr << "Smoke failure: unexpected early win state at tick " << world.tick << "\n"; return 5; }
  }

  if (o.smoke) {
    dom::sim::World replay; replay.width = o.mapW; replay.height = o.mapH; dom::sim::initialize_world(replay, o.seed);
    for (int i = 0; i < tickCount; ++i) {
      dom::ai::update_simple_ai(replay, 0);
      dom::ai::update_simple_ai(replay, 1);
      if (replay.researchStartedCount == 0 && (i % 100) == 0) {
        uint32_t cc = first_building(replay, 0, dom::sim::BuildingType::CityCenter);
        if (cc) dom::sim::enqueue_age_research(replay, 0, cc);
      }
      dom::sim::tick_world(replay, dom::core::kSimDeltaSeconds);
    }
    if (dom::sim::state_hash(world) != dom::sim::state_hash(replay)) { std::cerr << "Smoke failure: deterministic replay state hash mismatch\n"; return 13; }

    if (world.groupMoveCommandCount == 0) { std::cerr << "Smoke failure: no group move command executed\n"; return 14; }
    if (world.totalDamageDealtPermille == 0) { std::cerr << "Smoke failure: no combat damage dealt\n"; return 18; }
    if (world.unitDeathEvents == 0 && world.buildingDamageEvents == 0) { std::cerr << "Smoke failure: combat had no kills/building damage\n"; return 19; }
    const float avgSwitch = world.combatEngagementCount > 0 ? (float)world.targetSwitchCount / (float)world.combatEngagementCount : 0.0f;
    if (avgSwitch > 0.65f) { std::cerr << "Smoke failure: target switching thrash avg=" << avgSwitch << "\n"; return 20; }
    if (world.flowFieldGeneratedCount == 0) { std::cerr << "Smoke failure: no flow field generated\n"; return 15; }
    if (world.unitsReachedSlotCount < 6) { std::cerr << "Smoke failure: too few units reached destination slots\n"; return 16; }
    if (world.stuckMoveAssertions != 0) { std::cerr << "Smoke failure: stuck move assertion triggered\n"; return 17; }
    if (world.chaseLimitBreakCount > 200) { std::cerr << "Smoke failure: excessive chase leash breaks\n"; return 21; }
    if (world.territoryRecomputeCount == 0) { std::cerr << "Smoke failure: no territory recompute executed\n"; return 6; }
    if (world.aiDecisionCount == 0) { std::cerr << "Smoke failure: AI made no decisions\n"; return 7; }
    if (world.completedBuildingsCount < 1) { std::cerr << "Smoke failure: no completed building\n"; return 8; }
    if (world.trainedUnitsViaQueue < 1) { std::cerr << "Smoke failure: no trained unit via queue\n"; return 9; }
  }

  if (o.dumpHash) {
    std::cout << "map_hash=" << baselineHash << "\n";
    std::cout << "state_hash=" << dom::sim::state_hash(world) << "\n";
    if (o.navDebug) {
      std::cout << "nav_version=" << world.navVersion << "\n";
      std::cout << "flow_generated=" << world.flowFieldGeneratedCount << "\n";
      std::cout << "flow_cache_hits=" << world.flowFieldCacheHitCount << "\n";
      std::cout << "group_moves=" << world.groupMoveCommandCount << "\n";
    }
    if (o.combatDebug || dom::sim::combat_debug_enabled()) {
      std::cout << "engagements=" << world.combatEngagementCount << "\n";
      std::cout << "target_switches=" << world.targetSwitchCount << "\n";
      std::cout << "retreats=" << world.aiRetreatCount << "\n";
      std::cout << "damage_permille=" << world.totalDamageDealtPermille << "\n";
    }
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
