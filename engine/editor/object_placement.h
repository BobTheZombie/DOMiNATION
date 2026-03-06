#pragma once
#include "engine/sim/simulation.h"
#include <glm/vec2.hpp>

namespace dom::editor {
void place_editor_object(dom::sim::World& world, int tool, uint16_t owner, const glm::vec2& at);
}
