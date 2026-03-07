#include "engine/ui/hud.h"
#include "engine/ui/diplomacy_panel.h"
#include "engine/ui/production_menu.h"
#include "engine/ui/research_panel.h"
#include "engine/debug/debug_panels.h"
#include "engine/debug/debug_visuals.h"
#include "engine/editor/scenario_editor.h"
#include <SDL2/SDL.h>

#include <algorithm>

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::ui {
namespace {

const char* age_name(dom::sim::Age age) {
  switch (age) {
    case dom::sim::Age::Ancient: return "Ancient";
    case dom::sim::Age::Classical: return "Classical";
    case dom::sim::Age::Medieval: return "Medieval";
    case dom::sim::Age::Gunpowder: return "Industrial";
    case dom::sim::Age::Enlightenment: return "Modern";
    case dom::sim::Age::Industrial: return "Modern";
    case dom::sim::Age::Modern: return "Information";
    case dom::sim::Age::Information: return "Information";
  }
  return "Unknown";
}

const char* role_name(dom::sim::UnitRole role) {
  switch (role) {
    case dom::sim::UnitRole::Infantry: return "Infantry";
    case dom::sim::UnitRole::Ranged: return "Ranged";
    case dom::sim::UnitRole::Cavalry: return "Cavalry";
    case dom::sim::UnitRole::Siege: return "Siege";
    case dom::sim::UnitRole::Worker: return "Worker";
    case dom::sim::UnitRole::Building: return "Building";
    case dom::sim::UnitRole::Naval: return "Naval";
    case dom::sim::UnitRole::Transport: return "Transport";
    default: return "Unknown";
  }
}


const char* objective_state_name(dom::sim::ObjectiveState st) {
  switch (st) {
    case dom::sim::ObjectiveState::Inactive: return "Inactive";
    case dom::sim::ObjectiveState::Active: return "Active";
    case dom::sim::ObjectiveState::Completed: return "Completed";
    case dom::sim::ObjectiveState::Failed: return "Failed";
  }
  return "Inactive";
}

const char* supply_name(dom::sim::SupplyState state) {
  switch (state) {
    case dom::sim::SupplyState::InSupply: return "In Supply";
    case dom::sim::SupplyState::LowSupply: return "Low Supply";
    case dom::sim::SupplyState::OutOfSupply: return "Out of Supply";
  }
  return "Unknown";
}

} // namespace

void push_gameplay_notifications(dom::sim::World& world, UiState& uiState) {
  std::vector<dom::sim::GameplayEvent> events;
  dom::sim::consume_gameplay_events(events);
  for (const auto& ev : events) {
    bool include = ev.type == dom::sim::GameplayEventType::BuildingCompleted ||
                   ev.type == dom::sim::GameplayEventType::ObjectiveCompleted ||
                   ev.type == dom::sim::GameplayEventType::WarDeclared ||
                   ev.type == dom::sim::GameplayEventType::AllianceFormed ||
                   ev.type == dom::sim::GameplayEventType::AllianceBroken ||
                   ev.type == dom::sim::GameplayEventType::TradeAgreementCreated ||
                   ev.type == dom::sim::GameplayEventType::PostureChanged;
    if (!include) continue;
    uiState.notifications.push_back({ev.text.empty() ? "Strategic event updated" : ev.text, world.tick + 900u});
  }
  while (uiState.notifications.size() > 8) uiState.notifications.erase(uiState.notifications.begin());
}

void draw_hud(SDL_Window* window,
              dom::sim::World& world,
              const std::vector<uint32_t>& selected,
              UiState& uiState,
              const std::string& overlay) {
  (void)window;
#ifndef DOM_HAS_IMGUI
  (void)world;
  (void)selected;
  (void)uiState;
  (void)overlay;
  return;
#else
  if (world.players.empty()) return;
  const auto& p = world.players[0];
  uiState.notifications.erase(std::remove_if(uiState.notifications.begin(), uiState.notifications.end(),
    [&](const HudNotification& n) { return world.tick >= n.expireTick; }), uiState.notifications.end());

  ImGuiViewport* vp = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + 8.0f));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x - 16.0f, 74.0f));
  ImGuiWindowFlags barFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("RTS Resource Bar", nullptr, barFlags);
  ImGui::Text("Food %.0f | Wood %.0f | Metal %.0f | Wealth %.0f | Knowledge %.0f | Oil %.0f | Pop %u/%u | Age %s",
    p.resources[0], p.resources[1], p.resources[2], p.resources[3], p.resources[4], p.resources[5],
    p.popUsed, p.popCap, age_name(p.age));
  ImGui::Text("Civ %s | eco %.2f mil %.2f sci %.2f dip %.2f log %.2f strat %.2f", p.civilization.displayName.c_str(), p.civilization.economyBias, p.civilization.militaryBias, p.civilization.scienceBias, p.civilization.diplomacyBias, p.civilization.logisticsBias, p.civilization.strategicBias);
  if (!overlay.empty()) ImGui::TextUnformatted(overlay.c_str());
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + vp->Size.y - 170.0f));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x * 0.58f, 162.0f));
  ImGui::Begin("Selection", nullptr, barFlags);
  if (selected.empty()) {
    ImGui::TextUnformatted("No unit/building selected");
  } else {
    uint32_t id = selected.front();
    bool found = false;
    for (const auto& u : world.units) {
      if (u.id != id) continue;
      found = true;
      ImGui::Text("Unit #%u", u.id);
      ImGui::Text("Health: %.0f | Role: %s | Owner: P%u", u.hp, role_name(u.role), u.team);
      ImGui::Text("Supply: %s | Cargo: %zu", supply_name(u.supplyState), u.cargo.size());
      ImGui::Text("DefId: %s", u.definitionId.empty()?"(base)":u.definitionId.c_str());
      break;
    }
    if (!found) {
      for (const auto& b : world.buildings) {
        if (b.id != id) continue;
        ImGui::Text("Building #%u", b.id);
        ImGui::Text("Health: %.0f/%.0f | Owner: P%u", b.hp, b.maxHp, b.team);
        ImGui::Text("Queue items: %zu", b.queue.size());
        ImGui::Text("DefId: %s", b.definitionId.empty()?"(base)":b.definitionId.c_str());
        found = true;
        break;
      }
    }
    if (!found) ImGui::Text("Selection id #%u not visible", id);
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 290.0f, vp->Pos.y + vp->Size.y - 270.0f));
  ImGui::SetNextWindowSize(ImVec2(282.0f, 124.0f));
  ImGui::Begin("Minimap Frame", nullptr, barFlags);
  ImGui::TextUnformatted("Minimap active in renderer.");
  ImGui::SliderInt("Zoom indicator", &uiState.minimapZoomLevel, 1, 5);
  ImGui::TextUnformatted("[M] toggles minimap visibility.");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 390.0f, vp->Pos.y + 270.0f));
  ImGui::SetNextWindowSize(ImVec2(382.0f, 220.0f));
  ImGui::Begin("Mission", nullptr, barFlags);
  ImGui::Text("%s", world.mission.title.empty() ? "Skirmish Mission" : world.mission.title.c_str());
  if (!world.mission.briefing.empty()) ImGui::TextWrapped("%s", world.mission.briefing.c_str());
  ImGui::Separator();
  for (const auto& o : world.objectives) {
    if (!o.visible && o.state != dom::sim::ObjectiveState::Completed && o.state != dom::sim::ObjectiveState::Failed) continue;
    const bool primary = o.category == dom::sim::ObjectiveCategory::Primary || o.primary;
    ImGui::BulletText("[%s] %s (%s)", primary ? "P" : "S", o.title.c_str(), objective_state_name(o.state));
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 390.0f, vp->Pos.y + 90.0f));
  ImGui::SetNextWindowSize(ImVec2(382.0f, 170.0f));
  ImGui::Begin("Notifications", nullptr, barFlags);
  if (uiState.notifications.empty()) {
    ImGui::TextUnformatted("No active notifications");
  } else {
    for (const auto& n : uiState.notifications) ImGui::BulletText("%s", n.text.c_str());
  }
  ImGui::End();

  draw_production_menu(world, selected);
  draw_research_panel(world);
  draw_diplomacy_panel(world, uiState.showDiplomacyPanel, uiState.showOperationsPanel);
#endif
}

} // namespace dom::ui
