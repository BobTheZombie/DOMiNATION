#include "engine/debug/debug_panels.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::debug {
void draw_debug_panels(const dom::sim::World& world, DebugVisualState& state) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)state;
#else
  if (!ImGui::Begin("Debug Visualization")) { ImGui::End(); return; }
  ImGui::Checkbox("pathfinding grid", &state.pathfindingGrid);
  ImGui::Checkbox("chunk boundaries", &state.chunkBoundaries);
  ImGui::Checkbox("biome map", &state.biomeMap);
  ImGui::Checkbox("territory control", &state.territoryControl);
  ImGui::Checkbox("supply routes", &state.supplyRoutes);
  ImGui::Checkbox("operation targets", &state.operationTargets);
  ImGui::Checkbox("AI state", &state.aiState);
  ImGui::Text("Units: %zu | Buildings: %zu | Operations: %zu", world.units.size(), world.buildings.size(), world.operations.size());
  ImGui::End();

  if (!ImGui::Begin("Perf Graphs")) { ImGui::End(); return; }
  static float simHistory[120]{};
  static float navHistory[120]{};
  static float combatHistory[120]{};
  static float jobsHistory[120]{};
  static int idx = 0;
  auto profile = dom::sim::last_tick_profile();
  auto stats = dom::sim::last_simulation_stats();
  simHistory[idx] = static_cast<float>(profile.totalMs);
  navHistory[idx] = static_cast<float>(profile.navMs);
  combatHistory[idx] = static_cast<float>(profile.combatMs);
  jobsHistory[idx] = static_cast<float>(stats.jobCount);
  idx = (idx + 1) % 120;
  ImGui::PlotLines("simulation tick time", simHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("navigation time", navHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("combat time", combatHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("job system stats", jobsHistory, 120, idx, nullptr, 0.0f, 1000.0f, ImVec2(0, 50));
  ImGui::End();
#endif
}
} // namespace dom::debug
