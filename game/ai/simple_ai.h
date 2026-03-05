#pragma once
#include "engine/sim/simulation.h"
namespace dom::ai {
void set_attack_early(bool enabled);
void update_simple_ai(dom::sim::World& world, uint16_t team);
}
