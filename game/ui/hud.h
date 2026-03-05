#pragma once
#include "engine/sim/simulation.h"
struct SDL_Window;
namespace dom::ui {
void draw_hud(SDL_Window* window, const dom::sim::World& world);
}
