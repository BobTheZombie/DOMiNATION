#include "game/ai/simple_ai.h"
#include <vector>

namespace dom::ai {
void update_simple_ai(dom::sim::World& world, uint16_t team) {
  if (world.tick % 40 != 0) return;
  std::vector<uint32_t> mine;
  for (const auto& u : world.units) if (u.team == team) mine.push_back(u.id);
  if (mine.empty()) return;
  dom::sim::issue_move(world, team, mine, {22.0f, 20.0f});
  ++world.aiDecisionCount;
}
}
