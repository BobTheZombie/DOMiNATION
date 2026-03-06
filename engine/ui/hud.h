#pragma once

#include "engine/sim/simulation.h"
#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;

namespace dom::ui {

struct HudNotification {
  std::string text;
  uint32_t expireTick{0};
};

struct UiState {
  bool showHudDebug{false};
  bool showProductionMenu{false};
  bool showResearchPanel{false};
  bool showDiplomacyPanel{false};
  bool showOperationsPanel{false};
  bool showScenarioEditor{false};
  bool showDebugPanels{true};
  int minimapZoomLevel{2};
  std::vector<HudNotification> notifications;
};

void draw_hud(SDL_Window* window,
              dom::sim::World& world,
              const std::vector<uint32_t>& selected,
              UiState& uiState,
              const std::string& overlay = "");

void push_gameplay_notifications(dom::sim::World& world, UiState& uiState);

} // namespace dom::ui
