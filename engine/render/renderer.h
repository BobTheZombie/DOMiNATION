#pragma once
#include "engine/sim/simulation.h"
#include <glm/vec2.hpp>

namespace dom::render {
struct Camera {
  glm::vec2 center{64, 64};
  float zoom{8.0f};
};

bool init_renderer();
void draw(dom::sim::World& world, const Camera& camera, int width, int height);
uint32_t pick_unit(const dom::sim::World& world, const Camera& camera, int width, int height, glm::vec2 screen);
glm::vec2 screen_to_world(const Camera& camera, int width, int height, glm::vec2 screen);

void toggle_territory_overlay();
void toggle_border_overlay();
void toggle_fog_overlay();
} // namespace dom::render
