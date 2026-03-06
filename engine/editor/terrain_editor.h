#pragma once
#include "engine/sim/simulation.h"
#include <glm/vec2.hpp>

namespace dom::editor {
void apply_terrain_tool(dom::sim::World& world, int tool, int biome, float delta, int radius, const glm::vec2& center);
}
