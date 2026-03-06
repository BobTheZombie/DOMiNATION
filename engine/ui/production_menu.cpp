#include "engine/ui/production_menu.h"

#include <utility>

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

uint32_t selected_production_building(const dom::sim::World& world, const std::vector<uint32_t>& selected) {
  if (selected.empty()) return 0;
  for (const auto& b : world.buildings) if (b.id == selected.front()) return b.id;
  for (const auto& b : world.buildings) if (b.team == 0 && b.type == dom::sim::BuildingType::CityCenter) return b.id;
  return 0;
}

void queue_item_ui(dom::sim::World& world, dom::sim::Building& b, size_t idx) {
  auto& it = b.queue[idx];
  ImGui::PushID(static_cast<int>(idx));
  ImGui::Text("%zu) %s | %.1fs", idx + 1, unit_name(it.unitType), it.remaining);
  ImGui::SameLine();
  if (ImGui::SmallButton("Cancel")) {
    dom::sim::cancel_queue_item(world, 0, b.id, idx);
    ImGui::PopID();
    return;
  }
  ImGui::SameLine();
  if (idx > 0 && ImGui::SmallButton("Up")) std::swap(b.queue[idx], b.queue[idx - 1]);
  ImGui::SameLine();
  if (idx + 1 < b.queue.size() && ImGui::SmallButton("Down")) std::swap(b.queue[idx], b.queue[idx + 1]);
  ImGui::PopID();
}
}

void draw_production_menu(dom::sim::World& world, const std::vector<uint32_t>& selected) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)selected;
#else
  if (!world.uiTrainMenu) return;
  if (!ImGui::Begin("Production Menu", &world.uiTrainMenu)) { ImGui::End(); return; }

  uint32_t bid = selected_production_building(world, selected);
  if (!bid) {
    ImGui::TextUnformatted("Select a production structure.");
    ImGui::End();
    return;
  }

  dom::sim::Building* building = nullptr;
  for (auto& b : world.buildings) if (b.id == bid) building = &b;
  if (!building) {
    ImGui::End();
    return;
  }

  ImGui::Text("Building #%u", building->id);
  ImGui::TextUnformatted("Cost/Build Time: Worker 50/8s, Infantry 60/10s, Ranged 70/12s, Cavalry 90/16s, Siege 110/22s");

  if (building->type == dom::sim::BuildingType::Barracks) {
    if (ImGui::Button("Infantry")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Infantry);
    ImGui::SameLine();
    if (ImGui::Button("Ranged")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Archer);
    ImGui::SameLine();
    if (ImGui::Button("Special")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Siege);
  } else if (building->type == dom::sim::BuildingType::Port) {
    if (ImGui::Button("Cavalry")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Cavalry);
  } else if (building->type == dom::sim::BuildingType::Mine) {
    if (ImGui::Button("Siege")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Siege);
  } else {
    if (ImGui::Button("Worker")) dom::sim::enqueue_train_unit(world, 0, building->id, dom::sim::UnitType::Worker);
  }

  ImGui::SeparatorText("Queue");
  for (size_t i = 0; i < building->queue.size(); ++i) queue_item_ui(world, *building, i);

  ImGui::End();
#endif
}

} // namespace dom::ui
