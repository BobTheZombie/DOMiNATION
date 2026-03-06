#include "engine/ui/diplomacy_panel.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::ui {
namespace {
const char* relation_name(dom::sim::DiplomacyRelation rel) {
  switch (rel) {
    case dom::sim::DiplomacyRelation::Allied: return "ally";
    case dom::sim::DiplomacyRelation::War: return "war";
    default: return "neutral";
  }
}

const char* op_name(dom::sim::OperationType t) {
  switch (t) {
    case dom::sim::OperationType::AssaultCity: return "offensive operation";
    case dom::sim::OperationType::AmphibiousAssault: return "naval invasion";
    case dom::sim::OperationType::SecureRoute: return "supply disruption";
    case dom::sim::OperationType::CoastalBombard: return "strategic strike";
    default: return "operation";
  }
}
}

void draw_diplomacy_panel(dom::sim::World& world, bool showDiplomacyPanel, bool showOperationsPanel) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)showDiplomacyPanel; (void)showOperationsPanel;
#else
  if (world.players.empty()) return;
  if (showDiplomacyPanel) {
    if (ImGui::Begin("Diplomacy")) {
      ImGui::TextUnformatted("Players and relationship state");
      for (size_t i = 1; i < world.players.size(); ++i) {
        auto rel = world.diplomacy[0 * world.players.size() + i];
        ImGui::Text("Player %zu: %s", i, relation_name(rel));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Declare war")) dom::sim::declare_war(world, 0, static_cast<uint16_t>(i));
        ImGui::SameLine();
        if (ImGui::Button("Form alliance")) dom::sim::form_alliance(world, 0, static_cast<uint16_t>(i));
        ImGui::SameLine();
        if (ImGui::Button("Trade agreement")) dom::sim::establish_trade_agreement(world, 0, static_cast<uint16_t>(i));
        ImGui::PopID();
      }
    }
    ImGui::End();
  }

  if (!showOperationsPanel) return;
  if (!ImGui::Begin("Operations")) { ImGui::End(); return; }
  for (const auto& op : world.operations) {
    if (!op.active) continue;
    ImGui::BulletText("%s @ (%.1f, %.1f) assigned tick=%u", op_name(op.type), op.target.x, op.target.y, op.assignedTick);
  }
  ImGui::End();
#endif
}

} // namespace dom::ui
