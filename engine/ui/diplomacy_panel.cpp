#include "engine/ui/diplomacy_panel.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <string>
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

void draw_diplomacy_panel(dom::sim::World& world, bool showDiplomacyPanel, bool showOperationsPanel) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)showDiplomacyPanel; (void)showOperationsPanel;
#else
  if (world.players.empty()) return;
  if (showDiplomacyPanel) {
    if (ImGui::Begin("Diplomacy")) {
      ImGui::TextUnformatted("Players and relationship state");
      for (const auto& pl : world.players) {
        ImGui::SeparatorText((std::string("P") + std::to_string(pl.id) + " - " + pl.civilization.displayName).c_str());
        ImGui::TextWrapped("%s", pl.civilization.shortDescription.c_str());
        ImGui::Text("eco %.2f mil %.2f sci %.2f | road %.2f rail %.2f supply %.2f trade %.2f",
                    pl.civilization.economyBias, pl.civilization.militaryBias, pl.civilization.scienceBias,
                    pl.civilization.roadBonus, pl.civilization.railBonus, pl.civilization.supplyBonus, pl.civilization.tradeRouteBonus);
        ImGui::Text("doctrine aggr %.2f alliance %.2f trade %.2f tension %.2f",
                    pl.civilization.aggressionBias, pl.civilization.allianceBias, pl.civilization.tradeBias, pl.civilization.worldTensionResponseBias);
        if (pl.id < world.strategicDeterrence.size()) { const auto& ds = world.strategicDeterrence[pl.id]; ImGui::Text("deterrence cap=%d stockpile=%u ready=%u prep=%u alert=%u warning=%d retaliation=%d secondStrike=%d", ds.strategicCapabilityEnabled?1:0, ds.strategicStockpile, ds.strategicReadyCount, ds.strategicPreparingCount, ds.strategicAlertLevel, ds.launchWarningActive?1:0, ds.retaliationCapability?1:0, ds.secondStrikeCapability?1:0); }
      }
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
  ImGui::Text("Theaters: %zu | Objectives: %zu | ArmyGroups: %zu | NavalTF: %zu | AirWings: %zu",
              world.theaterCommands.size(), world.operationalObjectives.size(), world.armyGroups.size(), world.navalTaskForces.size(), world.airWings.size());
  for (const auto& t : world.theaterCommands) {
    ImGui::SeparatorText((std::string("Theater ") + std::to_string(t.theaterId)).c_str());
    ImGui::Text("owner=%u bounds=[%d,%d]-[%d,%d] supply=%.2f threat=%.2f", t.owner, t.bounds.x, t.bounds.y, t.bounds.z, t.bounds.w, t.supplyStatus, t.threatLevel);
    ImGui::Text("assigned forces: army=%zu naval=%zu air=%zu", t.assignedArmyGroups.size(), t.assignedNavalTaskForces.size(), t.assignedAirWings.size());
    for (uint32_t objectiveId : t.activeOperations) {
      auto it = std::find_if(world.operationalObjectives.begin(), world.operationalObjectives.end(), [&](const dom::sim::OperationalObjective& o){ return o.id == objectiveId; });
      if (it == world.operationalObjectives.end()) continue;
      ImGui::BulletText("objective %u %s required=%u start=%u dur=%u outcome=%u",
                        it->id, op_name(it->objectiveType), it->requiredForce, it->startTick, it->durationTicks, (unsigned)it->outcome);
    }
  }
  for (const auto& op : world.operations) {
    if (!op.active) continue;
    ImGui::BulletText("legacy op: %s @ (%.1f, %.1f) tick=%u", op_name(op.type), op.target.x, op.target.y, op.assignedTick);
  }
  ImGui::End();
#endif
}

} // namespace dom::ui
