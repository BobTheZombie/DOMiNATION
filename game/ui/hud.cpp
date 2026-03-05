#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <string>

namespace dom::ui {
void draw_hud(SDL_Window* window, const dom::sim::World& world) {
  const auto& p = world.players[0];
  std::string title = "DOMiNATION | Food " + std::to_string((int)p.resources[0]) +
    " Wood " + std::to_string((int)p.resources[1]) +
    " Metal " + std::to_string((int)p.resources[2]) +
    " Wealth " + std::to_string((int)p.resources[3]) +
    " Knowledge " + std::to_string((int)p.resources[4]) +
    " Oil " + std::to_string((int)p.resources[5]) +
    " | Age " + std::to_string((int)p.age) + (world.godMode ? " | GOD MODE" : "");
  SDL_SetWindowTitle(window, title.c_str());
}
}
