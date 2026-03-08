#include "engine/ui/research_panel.h"
#include "engine/ui/ui_theme.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::ui {
void draw_research_panel(dom::sim::World& world) {
#ifndef DOM_HAS_IMGUI
  (void)world;
#else
  if (!world.uiResearchMenu) return;
  if (!ImGui::Begin("Research & Age Progression", &world.uiResearchMenu)) { ImGui::End(); return; }

  static const char* timeline[] = {"Ancient", "Classical", "Medieval", "Industrial", "Modern", "Information"};
  theme::section_header("Age Timeline");
  for (int i = 0; i < 6; ++i) {
    bool done = static_cast<int>(world.players[0].age) >= i;
    ImGui::TextColored(done ? theme::state_color_success() : theme::state_color_warning(), "• %s", timeline[i]);
  }

  theme::section_header("Research Tree Snapshot");
  ImGui::Text("Active: Metallurgy, Irrigation, Logistics");
  ImGui::TextDisabled("Locked: Rail Industry (needs Industrial Age), Flight Doctrine (needs Modern Age)");
  ImGui::TextDisabled("Completed: Agriculture");
  ImGui::Text("Knowledge costs: 120 / 180 / 240");

  uint32_t cityCenter = 0;
  for (const auto& b : world.buildings) {
    if (b.team == 0 && b.type == dom::sim::BuildingType::CityCenter) { cityCenter = b.id; break; }
  }

  if (ImGui::Button("Queue Age Advancement") && cityCenter) dom::sim::enqueue_age_research(world, 0, cityCenter);

  if (cityCenter) {
    float progress = 0.0f;
    for (const auto& b : world.buildings) {
      if (b.id != cityCenter) continue;
      for (const auto& item : b.queue) {
        if (item.kind == dom::sim::QueueKind::AgeResearch) {
          progress = 1.0f - (item.remaining / 90.0f);
          break;
        }
      }
      break;
    }
    theme::section_header("Current Research");
    ImGui::ProgressBar(progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress), ImVec2(-1, 0), "Age research");
  }

  ImGui::End();
#endif
}
} // namespace dom::ui
