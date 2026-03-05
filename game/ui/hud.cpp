#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <string>

namespace dom::ui {
void draw_hud(SDL_Window* window, const dom::sim::World& world) {
  const auto& p = world.players[0];
  int capitals = 0;
  for (const auto& city : world.cities) if (city.capital) ++capitals;
  std::string title = "DOMiNATION | Food " + std::to_string((int)p.resources[0]) +
    " Wood " + std::to_string((int)p.resources[1]) +
    " Metal " + std::to_string((int)p.resources[2]) +
    " Wealth " + std::to_string((int)p.resources[3]) +
    " Knowledge " + std::to_string((int)p.resources[4]) +
    " Oil " + std::to_string((int)p.resources[5]) +
    " | Age " + std::to_string((int)p.age) +
    (world.godMode ? " | GOD MODE (All Visible)" : "") +
    " | Overlays [1]=Territory [2]=Borders [3]=Fog | Capitals " + std::to_string(capitals);
  SDL_SetWindowTitle(window, title.c_str());
}
}
