#include "engine/ui/research_panel.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::ui {
void draw_research_panel(dom::sim::World& world) {
#ifndef DOM_HAS_IMGUI
  (void)world;
#else
  if (!world.uiResearchMenu) return;
  if (!ImGui::Begin("Research & Ages", &world.uiResearchMenu)) { ImGui::End(); return; }

  static const char* timeline[] = {"Ancient", "Classical", "Medieval", "Industrial", "Modern", "Information"};
  ImGui::SeparatorText("Age Timeline");
  for (int i = 0; i < 6; ++i) {
    bool done = static_cast<int>(world.players[0].age) >= i;
    ImGui::BulletText("%s %s", timeline[i], done ? "(reached)" : "(locked)");
  }

  ImGui::SeparatorText("Research");
  ImGui::TextUnformatted("Available techs: Metallurgy, Irrigation, Logistics");
  ImGui::TextUnformatted("Locked techs: Rail Industry, Flight Doctrine");
  ImGui::TextUnformatted("Researched techs: Agriculture");
  ImGui::TextUnformatted("Costs: 120 / 180 / 240 knowledge");
  ImGui::TextUnformatted("Prerequisites: Age progress + previous nodes");

  uint32_t cityCenter = 0;
  for (const auto& b : world.buildings) {
    if (b.team == 0 && b.type == dom::sim::BuildingType::CityCenter) { cityCenter = b.id; break; }
  }
  if (ImGui::Button("Advance Age (queue)") && cityCenter) dom::sim::enqueue_age_research(world, 0, cityCenter);

  if (cityCenter) {
    for (const auto& b : world.buildings) {
      if (b.id != cityCenter) continue;
      float progress = 0.0f;
      for (const auto& item : b.queue) {
        if (item.kind == dom::sim::QueueKind::AgeResearch) {
          progress = 1.0f - (item.remaining / 90.0f);
          break;
        }
      }
      ImGui::ProgressBar(progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress), ImVec2(-1, 0), "Research progress");
      break;
    }
  }

  ImGui::End();
#endif
}
} // namespace dom::ui
