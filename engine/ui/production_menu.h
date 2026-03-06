#pragma once
#include "engine/sim/simulation.h"
#include <cstdint>
#include <vector>

namespace dom::ui {
void draw_production_menu(dom::sim::World& world, const std::vector<uint32_t>& selected);
}
