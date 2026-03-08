#pragma once

#include "engine/sim/simulation.h"

namespace dom::debug {

struct DebugVisualState {
  bool pathfindingGrid{false};
  bool chunkBoundaries{false};
  bool biomeMap{false};
  bool territoryControl{true};
  bool supplyRoutes{false};
  bool railNetwork{false};
  bool railSupply{false};
  bool railFreight{false};
  bool operationTargets{false};
  bool aiState{false};
  bool terrainMaterialOverlay{false};
  bool waterOverlay{false};
  bool entityPresentationDebug{false};
};

void draw_debug_panels(const dom::sim::World& world, DebugVisualState& state);

} // namespace dom::debug
