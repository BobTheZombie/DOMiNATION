#include "engine/editor/scenario_editor.h"
#include "engine/editor/object_placement.h"
#include "engine/editor/terrain_editor.h"

#include <algorithm>
#include <vector>

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::editor {
void draw_scenario_editor(dom::sim::World& world, const glm::vec2& cameraCenter, ScenarioEditorState& state) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)cameraCenter; (void)state;
#else
  if (!ImGui::Begin("Scenario Editor")) { ImGui::End(); return; }

  ImGui::SeparatorText("Terrain editing");
  const char* terrainTools[] = {"paint biome", "paint terrain height", "place rivers"};
  ImGui::Combo("Terrain Tool", &state.terrainTool, terrainTools, 3);
  ImGui::SliderInt("Biome", &state.selectedBiome, 0, static_cast<int>(dom::sim::BiomeType::Count) - 1);
  ImGui::SliderFloat("Height delta", &state.terrainDelta, -0.25f, 0.25f);
  ImGui::SliderInt("Brush radius", &state.brushRadius, 1, 8);
  if (ImGui::Button("Apply terrain at camera")) apply_terrain_tool(world, state.terrainTool, state.selectedBiome, state.terrainDelta, state.brushRadius, cameraCenter);

  ImGui::SeparatorText("Object placement");
  const char* objTools[] = {"place cities", "place buildings", "place resources", "place units"};
  ImGui::Combo("Placement", &state.objectTool, objTools, 4);
  ImGui::SliderInt("Owner", &state.selectedPlayer, 0, std::max(0, static_cast<int>(world.players.size()) - 1));
  if (ImGui::Button("Place at camera")) place_editor_object(world, state.objectTool, static_cast<uint16_t>(state.selectedPlayer), cameraCenter);

  ImGui::SeparatorText("Civilization setup");
  if (!world.players.empty()) {
    auto& p = world.players[std::clamp(state.selectedPlayer, 0, static_cast<int>(world.players.size()) - 1)];
    ImGui::Text("Player %u setup", p.id);
    ImGui::InputFloat("Food", &p.resources[0]);
    ImGui::InputFloat("Wood", &p.resources[1]);
    ImGui::InputFloat("Metal", &p.resources[2]);
    ImGui::InputFloat("Wealth", &p.resources[3]);
    ImGui::InputFloat("Knowledge", &p.resources[4]);
    if (world.players.size() > 1) {
      bool allied = dom::sim::players_allied(world, 0, 1);
      if (ImGui::Checkbox("P0<->P1 alliance", &allied)) {
        if (allied) dom::sim::form_alliance(world, 0, 1);
        else dom::sim::declare_war(world, 0, 1);
      }
    }
  }

  ImGui::SeparatorText("Mythic guardian sites");
  if (!world.guardianDefinitions.empty()) {
    std::vector<const char*> guardianNames;
    guardianNames.reserve(world.guardianDefinitions.size());
    for (const auto& g : world.guardianDefinitions) guardianNames.push_back(g.displayName.c_str());
    state.selectedGuardianDef = std::clamp(state.selectedGuardianDef, 0, (int)guardianNames.size() - 1);
    ImGui::Combo("Guardian", &state.selectedGuardianDef, guardianNames.data(), (int)guardianNames.size());
    if (ImGui::Button("Place guardian site at camera")) {
      const auto& gd = world.guardianDefinitions[static_cast<size_t>(state.selectedGuardianDef)];
      uint32_t nextId = 1;
      for (const auto& s : world.guardianSites) nextId = std::max(nextId, s.instanceId + 1);
      dom::sim::GuardianSiteInstance s{};
      s.instanceId = nextId;
      s.guardianId = gd.guardianId;
      s.siteType = gd.siteType;
      s.pos = cameraCenter;
      s.regionId = -1;
      s.scenarioPlaced = true;
      world.guardianSites.push_back(std::move(s));
    }
  } else {
    ImGui::TextUnformatted("No guardian definitions loaded.");
  }

  ImGui::SeparatorText("Save scenario");
  ImGui::InputText("File", state.saveName, sizeof(state.saveName));
  if (ImGui::Button("Save scenarios/*.json")) {
    std::string out = std::string("scenarios/") + state.saveName;
    std::string err;
    if (!dom::sim::save_scenario_file(out, world, err)) ImGui::Text("Save failed: %s", err.c_str());
    else ImGui::Text("Saved: %s", out.c_str());
  }

  ImGui::End();
#endif
}
} // namespace dom::editor
