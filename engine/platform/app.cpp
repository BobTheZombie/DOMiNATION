#include "engine/platform/app.h"
#include "engine/core/time.h"
#include "engine/render/renderer.h"
#include "engine/sim/simulation.h"
#include "game/ai/simple_ai.h"
#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <glm/vec2.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <limits>

namespace {
struct CliOptions {
  bool headless{false};
  bool smoke{false};
  bool dumpHash{false};
  uint32_t seed{1337};
  int ticks{-1};
  int mapW{128};
  int mapH{128};
};

bool parse_int(const std::string& s, int& out) {
  try { out = std::stoi(s); return true; } catch (...) { return false; }
}

bool parse_u32(const std::string& s, uint32_t& out) {
  try {
    unsigned long v = std::stoul(s);
    if (v > std::numeric_limits<uint32_t>::max()) return false;
    out = static_cast<uint32_t>(v);
    return true;
  } catch (...) { return false; }
}

bool parse_cli(int argc, char** argv, CliOptions& o) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--headless") o.headless = true;
    else if (a == "--smoke") o.smoke = true;
    else if (a == "--dump-hash") o.dumpHash = true;
    else if (a == "--ticks" && i + 1 < argc) {
      if (!parse_int(argv[++i], o.ticks) || o.ticks < 0) return false;
    } else if (a == "--seed" && i + 1 < argc) {
      if (!parse_u32(argv[++i], o.seed)) return false;
    } else if (a == "--map-size" && i + 1 < argc) {
      std::string v = argv[++i];
      auto xPos = v.find('x');
      if (xPos == std::string::npos) return false;
      int w = 0, h = 0;
      if (!parse_int(v.substr(0, xPos), w) || !parse_int(v.substr(xPos + 1), h)) return false;
      if (w < 16 || h < 16 || w > 1024 || h > 1024) return false;
      o.mapW = w;
      o.mapH = h;
    } else {
      std::cerr << "Unknown or malformed argument: " << a << "\n";
      return false;
    }
  }
  return true;
}

int run_headless(const CliOptions& o) {
  dom::sim::World world;
  world.width = o.mapW;
  world.height = o.mapH;
  dom::sim::initialize_world(world, o.seed);
  const uint64_t baselineHash = dom::sim::map_setup_hash(world);

  dom::sim::World second;
  second.width = o.mapW;
  second.height = o.mapH;
  dom::sim::initialize_world(second, o.seed);
  if (o.smoke && baselineHash != dom::sim::map_setup_hash(second)) {
    std::cerr << "Smoke failure: map hash mismatch for identical seed\n";
    return 2;
  }

  const int tickCount = o.ticks >= 0 ? o.ticks : 600;
  for (int i = 0; i < tickCount; ++i) {
    dom::ai::update_simple_ai(world, 1);
    dom::sim::tick_world(world, dom::core::kSimDeltaSeconds);

    if (!o.smoke) continue;
    for (const auto& u : world.units) {
      if (u.id == 0 || !std::isfinite(u.hp) || !std::isfinite(u.pos.x) || !std::isfinite(u.pos.y)) {
        std::cerr << "Smoke failure: invalid unit state at tick " << i << "\n";
        return 3;
      }
      if (u.pos.x < -2.0f || u.pos.y < -2.0f || u.pos.x > world.width + 2.0f || u.pos.y > world.height + 2.0f) {
        std::cerr << "Smoke failure: out-of-bounds unit at tick " << i << "\n";
        return 4;
      }
    }

    if (world.gameOver && world.tick < 20 * 600) {
      std::cerr << "Smoke failure: unexpected early win state at tick " << world.tick << "\n";
      return 5;
    }
  }

  if (o.smoke) {
    if (world.territoryRecomputeCount == 0) {
      std::cerr << "Smoke failure: no territory recompute executed\n";
      return 6;
    }
    if (world.aiDecisionCount == 0) {
      std::cerr << "Smoke failure: AI made no decisions\n";
      return 7;
    }
  }

  if (o.dumpHash) {
    std::cout << "map_hash=" << baselineHash << "\n";
    std::cout << "state_hash=" << dom::sim::state_hash(world) << "\n";
  }
  return 0;
}
} // namespace

int run_app(int argc, char** argv) {
  CliOptions opts;
  if (!parse_cli(argc, argv, opts)) return 1;
  if (opts.headless) return run_headless(opts);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_Window* window = SDL_CreateWindow("DOMiNATION RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_Quit();
    return 1;
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  if (!ctx) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);
  dom::render::init_renderer();

  dom::sim::World world;
  world.width = opts.mapW;
  world.height = opts.mapH;
  dom::sim::initialize_world(world, opts.seed);
  dom::render::Camera camera;
  std::vector<uint32_t> selected;

  bool running = true;
  Uint64 prev = SDL_GetPerformanceCounter();
  float accum = 0.0f;
  while (running) {
    Uint64 now = SDL_GetPerformanceCounter();
    float frameDt = (now - prev) / static_cast<float>(SDL_GetPerformanceFrequency());
    prev = now;
    accum += frameDt;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_g) dom::sim::toggle_god_mode(world);
        if (e.key.keysym.sym == SDLK_1) dom::render::toggle_territory_overlay();
        if (e.key.keysym.sym == SDLK_2) dom::render::toggle_border_overlay();
        if (e.key.keysym.sym == SDLK_3) dom::render::toggle_fog_overlay();
      }
      if (e.type == SDL_MOUSEWHEEL) {
        camera.zoom = std::clamp(camera.zoom - e.wheel.y * (world.godMode ? 4.0f : 1.2f), 4.0f, world.godMode ? 160.0f : 35.0f);
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int w, h; SDL_GetWindowSize(window, &w, &h);
        uint32_t pick = dom::render::pick_unit(world, camera, w, h, {(float)e.button.x, (float)e.button.y});
        selected.clear();
        for (auto& u : world.units) u.selected = false;
        if (pick) {
          selected.push_back(pick);
          for (auto& u : world.units) if (u.id == pick) u.selected = true;
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT && !selected.empty()) {
        int w, h; SDL_GetWindowSize(window, &w, &h);
        auto target = dom::render::screen_to_world(camera, w, h, {(float)e.button.x, (float)e.button.y});
        dom::sim::issue_move(world, 0, selected, target);
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
    dom::render::draw(world, camera, w, h);
    dom::ui::draw_hud(window, world);
    SDL_GL_SwapWindow(window);
  }

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
