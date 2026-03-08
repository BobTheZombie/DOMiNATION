#include "engine/ui/diplomacy_panel.h"
#include "engine/ui/ui_theme.h"
#include "engine/ui/ui_icons.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <string>
#endif

namespace dom::ui {
namespace {
const char* relation_name(dom::sim::DiplomacyRelation rel) {
  switch (rel) {
    case dom::sim::DiplomacyRelation::Allied: return "allied";
    case dom::sim::DiplomacyRelation::War: return "at war";
    default: return "neutral";
  }
}

const char* op_name(dom::sim::OperationType t) {
  switch (t) {
    case dom::sim::OperationType::AssaultCity: return "offensive";
    case dom::sim::OperationType::DefendBorder: return "defensive line";
    case dom::sim::OperationType::Encirclement: return "encirclement";
    case dom::sim::OperationType::NavalBlockade: return "naval blockade";
    case dom::sim::OperationType::StrategicBombing: return "strategic bombing";
    case dom::sim::OperationType::MissileStrikeCampaign: return "missile strike";
    case dom::sim::OperationType::AmphibiousAssault: return "naval invasion";
    case dom::sim::OperationType::SecureRoute: return "supply corridor";
    case dom::sim::OperationType::CoastalBombard: return "coastal bombard";
    default: return "operation";
  }
}
}

int bloc_index_for_player(const dom::sim::World& world, uint16_t player) {
  for (size_t i = 0; i < world.allianceBlocs.size(); ++i) {
    const auto& b = world.allianceBlocs[i];
    if (b.lifecycleState != 1) continue;
    if (std::find(b.members.begin(), b.members.end(), player) != b.members.end()) return static_cast<int>(i);
  }
  return -1;
}

void draw_diplomacy_panel(dom::sim::World& world, bool showDiplomacyPanel, bool showOperationsPanel) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)showDiplomacyPanel; (void)showOperationsPanel;
#else
  if (world.players.empty()) return;
  if (showDiplomacyPanel) {
    if (ImGui::Begin("Diplomacy Overview")) {
      theme::section_header("Civilizations & Relations");
      for (const auto& pl : world.players) {
        const auto emblem = icons::civ_emblem_icon_id(world, pl.id);
        ImGui::SeparatorText((std::string(icons::glyph_for_icon(emblem)) + " P" + std::to_string(pl.id) + " · " + pl.civilization.displayName).c_str());
        ImGui::TextWrapped("%s", pl.civilization.shortDescription.c_str());
        ImGui::Text("Ideology: %s / %s", pl.civilization.ideology.primary.c_str(), pl.civilization.ideology.secondary.c_str());
        int bi = bloc_index_for_player(world, pl.id);
        if (bi >= 0) {
          const auto& b = world.allianceBlocs[(size_t)bi];
          ImGui::Text("%s Bloc %s | cohesion %.2f threat %.2f", icons::glyph_for_icon("ui_icon_diplomacy"), b.blocId.c_str(), b.cohesion, b.threatLevel);
        } else {
          ImGui::TextDisabled("Bloc: none");
        }
        if (pl.id < world.strategicDeterrence.size()) {
          const auto& ds = world.strategicDeterrence[pl.id];
          ImGui::Text("%s Deterrence: stockpile %u | ready %u | alert %u", icons::glyph_for_icon("ui_icon_warning_strategic"), ds.strategicStockpile, ds.strategicReadyCount, ds.strategicAlertLevel);
        }
      }

      theme::section_header("Treaties / Direct Actions");
      for (size_t i = 1; i < world.players.size(); ++i) {
        auto rel = world.diplomacy[0 * world.players.size() + i];
        ImGui::Text("%s P0 ↔ P%zu: %s", icons::glyph_for_icon("ui_icon_diplomacy"), i, relation_name(rel));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Declare war")) dom::sim::declare_war(world, 0, static_cast<uint16_t>(i));
        ImGui::SameLine();
        if (ImGui::Button("Alliance")) dom::sim::form_alliance(world, 0, static_cast<uint16_t>(i));
        ImGui::SameLine();
        if (ImGui::Button("Trade")) dom::sim::establish_trade_agreement(world, 0, static_cast<uint16_t>(i));
        ImGui::PopID();
      }
    }
    ImGui::End();
  }

  if (!showOperationsPanel) return;
  if (!ImGui::Begin("Operations & Theaters")) { ImGui::End(); return; }
  theme::section_header("Strategic Posture");
  ImGui::Text("Theaters %zu | Objectives %zu | Army Groups %zu | Naval TF %zu | Air Wings %zu",
              world.theaterCommands.size(), world.operationalObjectives.size(), world.armyGroups.size(), world.navalTaskForces.size(), world.airWings.size());
  for (const auto& t : world.theaterCommands) {
    ImGui::SeparatorText((std::string("Theater ") + std::to_string(t.theaterId)).c_str());
    ImGui::Text("Owner P%u | supply %.2f threat %.2f", t.owner, t.supplyStatus, t.threatLevel);
    ImGui::Text("Forces: army %zu naval %zu air %zu", t.assignedArmyGroups.size(), t.assignedNavalTaskForces.size(), t.assignedAirWings.size());
    for (uint32_t objectiveId : t.activeOperations) {
      auto it = std::find_if(world.operationalObjectives.begin(), world.operationalObjectives.end(), [&](const dom::sim::OperationalObjective& o){ return o.id == objectiveId; });
      if (it == world.operationalObjectives.end()) continue;
      ImGui::BulletText("%s Objective %u · %s · required %u", icons::glyph_for_icon("ui_icon_objective"), it->id, op_name(it->objectiveType), it->requiredForce);
    }
  }
  ImGui::End();
#endif
}

} // namespace dom::ui
