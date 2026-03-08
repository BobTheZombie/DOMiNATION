#include "engine/ui/production_menu.h"
#include "engine/ui/ui_theme.h"

#include <utility>
#include <string>

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::ui {
namespace {
const char* unit_name(dom::sim::UnitType t) {
  switch (t) {
    case dom::sim::UnitType::Worker: return "Worker";
    case dom::sim::UnitType::Infantry: return "Infantry";
    case dom::sim::UnitType::Archer: return "Ranged";
    case dom::sim::UnitType::Cavalry: return "Cavalry";
    case dom::sim::UnitType::Siege: return "Siege";
    default: return "Special";
  }
}

std::string unit_button_label(const dom::sim::World& world, uint16_t team, dom::sim::UnitType type) {
  const std::string defId = (team < world.players.size() && !world.players[team].civilization.uniqueUnitDefs[(size_t)type].empty())
    ? world.players[team].civilization.uniqueUnitDefs[(size_t)type]
    : unit_name(type);
  const auto info = dom::sim::unit_content_presentation(world, team, type, defId);
  return info.displayName + " [" + info.iconId + "]";
}

uint32_t selected_production_building(const dom::sim::World& world, const std::vector<uint32_t>& selected) {
  if (selected.empty()) return 0;
  for (const auto& b : world.buildings) if (b.id == selected.front()) return b.id;
  for (const auto& b : world.buildings) if (b.team == 0 && b.type == dom::sim::BuildingType::CityCenter) return b.id;
  return 0;
}

void queue_item_ui(dom::sim::World& world, dom::sim::Building& b, size_t idx) {
#ifdef DOM_HAS_IMGUI
  auto& it = b.queue[idx];
  ImGui::PushID(static_cast<int>(idx));
  ImGui::Text("%zu) %s", idx + 1, unit_name(it.unitType));
  ImGui::SameLine();
  ImGui::TextDisabled("%.1fs", it.remaining);
  ImGui::SameLine();
  if (ImGui::SmallButton("Cancel")) {
    dom::sim::cancel_queue_item(world, 0, b.id, idx);
    ImGui::PopID();
    return;
  }
  ImGui::PopID();
#else
  (void)world; (void)b; (void)idx;
#endif
}

void train_card(dom::sim::World& world, uint16_t team, uint32_t buildingId, dom::sim::UnitType type, const char* costInfo, bool locked, const char* lockReason) {
  ImGui::PushID(static_cast<int>(type));
  ImGui::BeginChild("card", ImVec2(0.0f, 58.0f), true);
  ImGui::TextUnformatted(unit_button_label(world, team, type).c_str());
  ImGui::TextDisabled("%s", costInfo);
  if (locked) {
    ImGui::TextColored(theme::state_color_warning(), "Locked: %s", lockReason);
  } else if (ImGui::Button("Queue")) {
    dom::sim::enqueue_train_unit(world, team, buildingId, type);
  }
  ImGui::EndChild();
  ImGui::PopID();
}
}

void draw_production_menu(dom::sim::World& world, const std::vector<uint32_t>& selected) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)selected;
#else
  if (!world.uiTrainMenu) return;
  if (!ImGui::Begin("Production Command", &world.uiTrainMenu)) { ImGui::End(); return; }

  uint32_t bid = selected_production_building(world, selected);
  if (!bid) {
    ImGui::TextUnformatted("Select a production structure.");
    ImGui::End();
    return;
  }

  dom::sim::Building* building = nullptr;
  for (auto& b : world.buildings) if (b.id == bid) building = &b;
  if (!building) { ImGui::End(); return; }

  ImGui::Text("Building #%u | Queue %zu", building->id, building->queue.size());
  const uint16_t ownerTeam = building->team;
  theme::section_header("Build Cards");

  if (building->type == dom::sim::BuildingType::Barracks) {
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Infantry, "Cost 60 | 10s", false, "");
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Archer, "Cost 70 | 12s", false, "");
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Siege, "Cost 110 | 22s", building->queue.size() > 4, "Queue full");
  } else if (building->type == dom::sim::BuildingType::Port) {
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Cavalry, "Cost 90 | 16s", false, "");
  } else if (building->type == dom::sim::BuildingType::Mine) {
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Siege, "Cost 110 | 22s", false, "");
  } else if (building->type == dom::sim::BuildingType::SteelMill || building->type == dom::sim::BuildingType::Refinery || building->type == dom::sim::BuildingType::MunitionsPlant || building->type == dom::sim::BuildingType::MachineWorks || building->type == dom::sim::BuildingType::ElectronicsLab || building->type == dom::sim::BuildingType::FactoryHub) {
    theme::section_header("Industrial Output");
    ImGui::Text("Recipe index: %u (%s)", building->factory.recipeIndex, building->factory.active ? "active" : "idle");
    if (building->factory.blocked) ImGui::TextColored(theme::state_color_warning(), "Blocked: missing inputs");
    if (ImGui::Button("Steel")) building->factory.recipeIndex = 0;
    ImGui::SameLine(); if (ImGui::Button("Fuel")) building->factory.recipeIndex = 1;
    ImGui::SameLine(); if (ImGui::Button("Munitions")) building->factory.recipeIndex = 2;
    if (ImGui::Button("Machine Parts")) building->factory.recipeIndex = 3;
    ImGui::SameLine(); if (ImGui::Button("Electronics")) building->factory.recipeIndex = 4;
  } else {
    train_card(world, ownerTeam, building->id, dom::sim::UnitType::Worker, "Cost 50 | 8s", false, "");
  }

  theme::section_header("Queue State");
  if (building->queue.empty()) ImGui::TextDisabled("Queue is empty");
  for (size_t i = 0; i < building->queue.size(); ++i) queue_item_ui(world, *building, i);

  ImGui::End();
#endif
}

} // namespace dom::ui
