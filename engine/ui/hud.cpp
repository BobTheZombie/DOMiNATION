#include "engine/ui/hud.h"
#include "engine/ui/diplomacy_panel.h"
#include "engine/ui/production_menu.h"
#include "engine/ui/research_panel.h"
#include "engine/ui/ui_theme.h"
#include "engine/debug/debug_panels.h"
#include "engine/debug/debug_visuals.h"
#include "engine/editor/scenario_editor.h"
#include <SDL2/SDL.h>

#include <algorithm>
#include <glm/geometric.hpp>

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

const char* objective_state_name(dom::sim::ObjectiveState st) {
  switch (st) {
    case dom::sim::ObjectiveState::Inactive: return "Inactive";
    case dom::sim::ObjectiveState::Active: return "Active";
    case dom::sim::ObjectiveState::Completed: return "Completed";
    case dom::sim::ObjectiveState::Failed: return "Failed";
  }
  return "Inactive";
}

const char* world_event_state_name(dom::sim::WorldEventState st) {
  switch (st) {
    case dom::sim::WorldEventState::Inactive: return "Inactive";
    case dom::sim::WorldEventState::Active: return "Active";
    case dom::sim::WorldEventState::Resolved: return "Resolved";
  }
  return "Inactive";
}

const char* world_event_category_name(dom::sim::WorldEventCategory c) {
  switch (c) {
    case dom::sim::WorldEventCategory::Climate: return "Climate";
    case dom::sim::WorldEventCategory::Health: return "Health";
    case dom::sim::WorldEventCategory::Economic: return "Economic";
    case dom::sim::WorldEventCategory::Political: return "Political";
    case dom::sim::WorldEventCategory::Industrial: return "Industrial";
    case dom::sim::WorldEventCategory::Strategic: return "Strategic";
    case dom::sim::WorldEventCategory::Mythic: return "Mythic";
  }
  return "Climate";
}

const char* supply_name(dom::sim::SupplyState state) {
  switch (state) {
    case dom::sim::SupplyState::InSupply: return "In Supply";
    case dom::sim::SupplyState::LowSupply: return "Low Supply";
    case dom::sim::SupplyState::OutOfSupply: return "Out of Supply";
  }
  return "Unknown";
}

ImVec4 objective_color(dom::sim::ObjectiveState st) {
  if (st == dom::sim::ObjectiveState::Completed) return theme::state_color_success();
  if (st == dom::sim::ObjectiveState::Failed) return theme::state_color_failure();
  if (st == dom::sim::ObjectiveState::Active) return theme::state_color_info();
  return ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
}

void draw_selection_summary(dom::sim::World& world, const std::vector<uint32_t>& selected) {
  theme::section_header("Selection & Context");
  if (selected.empty()) {
    ImGui::TextUnformatted("No active selection");
    return;
  }
  ImGui::Text("Selected: %zu", selected.size());
  uint32_t id = selected.front();
  for (const auto& u : world.units) {
    if (u.id != id) continue;
    ImGui::Text("Unit #%u | Team P%u", u.id, u.team);
    ImGui::Text("HP %.0f | Supply %s | Cargo %zu", u.hp, supply_name(u.supplyState), u.cargo.size());
    const auto info = dom::sim::unit_content_presentation(world, u.team, u.type, u.definitionId);
    ImGui::Text("%s [%s]", info.displayName.c_str(), info.iconId.c_str());
    return;
  }
  for (const auto& b : world.buildings) {
    if (b.id != id) continue;
    ImGui::Text("Building #%u | Team P%u", b.id, b.team);
    ImGui::Text("HP %.0f / %.0f | Queue %zu", b.hp, b.maxHp, b.queue.size());
    const auto info = dom::sim::building_content_presentation(world, b.team, b.type, b.definitionId);
    ImGui::Text("%s [%s]", info.displayName.c_str(), info.iconId.c_str());
    if (b.type == dom::sim::BuildingType::FactoryHub || b.type == dom::sim::BuildingType::SteelMill || b.type == dom::sim::BuildingType::Refinery) {
      ImGui::Text("Factory %s | throughput %.2f", b.factory.active ? "active" : "idle", b.factory.throughputBonus);
    }
    return;
  }
  ImGui::Text("Selection id #%u not visible", id);
}

void draw_top_bar(const ImGuiViewport* vp, dom::sim::World& world, const std::string& overlay) {
  const auto& p = world.players[0];
  ImGuiWindowFlags f = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + 8.0f));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x - 16.0f, 95.0f));
  if (!ImGui::Begin("Top Strategy Bar", nullptr, f)) { ImGui::End(); return; }

  ImGui::Text("%s | Age %s | Population %u/%u", p.civilization.displayName.c_str(), age_name(p.age), p.popUsed, p.popCap);
  ImGui::Separator();
  ImGui::Text("Food %.0f  Wood %.0f  Metal %.0f  Wealth %.0f  Knowledge %.0f  Oil %.0f",
              p.resources[0], p.resources[1], p.resources[2], p.resources[3], p.resources[4], p.resources[5]);
  ImGui::Text("Steel %.1f  Fuel %.1f  Munitions %.1f  Machine %.1f  Electronics %.1f",
              p.refinedGoods[0], p.refinedGoods[1], p.refinedGoods[2], p.refinedGoods[3], p.refinedGoods[4]);
  theme::state_text("World tension:", (std::to_string(static_cast<int>(world.worldTension * 100.0f)) + "%").c_str(), world.worldTension > 0.75f ? theme::state_color_warning() : theme::state_color_info());
  if (world.armageddonActive) {
    ImGui::SameLine();
    ImGui::TextColored(theme::state_color_failure(), "[ARMAGEDDON ACTIVE]");
  }
  if (!overlay.empty()) ImGui::TextColored(theme::state_color_info(), "%s", overlay.c_str());
  ImGui::End();
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
                   ev.type == dom::sim::GameplayEventType::PostureChanged ||
                   ev.type == dom::sim::GameplayEventType::GuardianDiscovered ||
                   ev.type == dom::sim::GameplayEventType::GuardianJoined ||
                   ev.type == dom::sim::GameplayEventType::GuardianKilled;
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
  (void)world; (void)selected; (void)uiState; (void)overlay;
  return;
#else
  if (world.players.empty()) return;
  uiState.notifications.erase(std::remove_if(uiState.notifications.begin(), uiState.notifications.end(),
    [&](const HudNotification& n) { return world.tick >= n.expireTick; }), uiState.notifications.end());

  ImGuiViewport* vp = ImGui::GetMainViewport();
  const ImVec4 accent = theme::civ_accent(world, 0);
  const float uiScale = ImGui::GetIO().FontGlobalScale > 0.0f ? ImGui::GetIO().FontGlobalScale : 1.0f;
  theme::ScopedHudTheme scopedTheme(uiScale, accent);

  draw_top_bar(vp, world, overlay);

  ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 420.0f, vp->Pos.y + 110.0f));
  ImGui::SetNextWindowSize(ImVec2(412.0f, 390.0f));
  if (ImGui::Begin("Strategic Alerts", nullptr, panelFlags)) {
    theme::section_header("Alerts");
    if (world.armageddonActive) ImGui::TextColored(theme::state_color_failure(), "Nuclear Armageddon escalation active");
    if (world.worldTension >= 0.9f) ImGui::TextColored(theme::state_color_warning(), "World tension threshold nearing critical");
    for (const auto& n : uiState.notifications) ImGui::BulletText("%s", n.text.c_str());

    theme::section_header("Mission Objectives");
    auto draw_group = [&](const char* label, dom::sim::ObjectiveCategory category) {
      ImGui::TextUnformatted(label);
      for (const auto& o : world.objectives) {
        if (o.category != category && !(category == dom::sim::ObjectiveCategory::Primary && o.primary)) continue;
        if (!o.visible && o.state != dom::sim::ObjectiveState::Completed && o.state != dom::sim::ObjectiveState::Failed) continue;
        ImGui::TextColored(objective_color(o.state), "• %s", o.title.c_str());
        if (!o.progressText.empty() || o.progressValue > 0.0f) ImGui::TextDisabled("  %s %.2f", o.progressText.c_str(), o.progressValue);
      }
    };
    draw_group("Primary", dom::sim::ObjectiveCategory::Primary);
    draw_group("Secondary", dom::sim::ObjectiveCategory::Secondary);
    draw_group("Hidden/Revealed", dom::sim::ObjectiveCategory::HiddenOptional);

    theme::section_header("Crisis/Event Feed");
    int shown = 0;
    for (auto it = world.worldEvents.rbegin(); it != world.worldEvents.rend() && shown < 5; ++it, ++shown) {
      ImGui::BulletText("%s [%s] %s", it->displayName.c_str(), world_event_category_name(it->category), world_event_state_name(it->state));
    }
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + vp->Size.y - 230.0f));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x * 0.65f, 222.0f));
  if (ImGui::Begin("Command Deck", nullptr, panelFlags)) {
    draw_selection_summary(world, selected);
    theme::section_header("Message Log");
    int shown = 0;
    for (auto it = world.missionMessages.rbegin(); it != world.missionMessages.rend() && shown < 5; ++it, ++shown) {
      ImGui::BulletText("[%u] %s: %s", it->tick, it->title.empty() ? it->category.c_str() : it->title.c_str(), it->body.c_str());
    }
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 320.0f, vp->Pos.y + vp->Size.y - 250.0f));
  ImGui::SetNextWindowSize(ImVec2(312.0f, 242.0f));
  if (ImGui::Begin("Minimap", nullptr, panelFlags)) {
    ImGui::TextUnformatted("[ Strategic Minimap ]");
    ImGui::Separator();
    ImGui::TextUnformatted("Minimap active in renderer.");
    ImGui::SliderInt("Zoom", &uiState.minimapZoomLevel, 1, 5);
    ImGui::Text("Viewport marker: centered");
    ImGui::Text("Markers: theaters %zu | crises %u", world.theaterCommands.size(), world.activeWorldEventCount);
  }
  ImGui::End();

  draw_production_menu(world, selected);
  draw_research_panel(world);
  draw_diplomacy_panel(world, uiState.showDiplomacyPanel, uiState.showOperationsPanel);
#endif
}

} // namespace dom::ui
