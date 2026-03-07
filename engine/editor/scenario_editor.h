#pragma once

#include "engine/sim/simulation.h"
#include <glm/vec2.hpp>

namespace dom::editor {

struct ScenarioEditorState {
  int terrainTool{0};
  int objectTool{0};
  int selectedPlayer{0};
  int selectedBiome{0};
  float terrainDelta{0.1f};
  int brushRadius{2};
  int selectedGuardianDef{0};
  char saveName[96]{"editor_scenario.json"};
};

void draw_scenario_editor(dom::sim::World& world, const glm::vec2& cameraCenter, ScenarioEditorState& state);

} // namespace dom::editor
