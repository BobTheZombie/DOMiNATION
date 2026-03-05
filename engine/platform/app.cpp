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

int run_app() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_Window* window = SDL_CreateWindow("DOMiNATION RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1);
  dom::render::init_renderer();

  dom::sim::World world;
  dom::sim::initialize_world(world, 1337);
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
      }
      if (e.type == SDL_MOUSEWHEEL) {
        camera.zoom = std::clamp(camera.zoom - e.wheel.y * (world.godMode ? 4.0f : 1.2f), 4.0f, world.godMode ? 120.0f : 35.0f);
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
