#pragma once

#include "engine/sim/simulation.h"
#include "engine/ui/hud.h"
#include <string>
#include <vector>

namespace dom::ui::alerts {

enum class Severity : uint8_t { Info, Warning, Critical, Apocalyptic };

struct StrategicAlert {
  uint64_t orderKey{0};
  std::string iconId;
  std::string title;
  std::string subtitle;
  std::string styleId;
  std::string speaker;
  std::string emblemIconId;
  Severity severity{Severity::Info};
};

std::vector<StrategicAlert> build_alert_queue(const dom::sim::World& world, const UiState& uiState);
const char* severity_name(Severity s);

} // namespace dom::ui::alerts
