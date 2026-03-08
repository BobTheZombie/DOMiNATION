#pragma once

#include "engine/sim/simulation.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace dom::ui::icons {

struct PresentationCounters {
  uint64_t iconResolveCount{0};
  uint64_t markerResolveCount{0};
  uint64_t alertResolveCount{0};
  uint64_t presentationFallbackCount{0};
};

void reset_frame_counters();
const PresentationCounters& presentation_counters();

std::string civ_emblem_icon_id(const dom::sim::World& world, uint16_t team);
std::string resolve_icon_id(const dom::sim::World& world,
                            uint16_t team,
                            const std::string& exactIconId,
                            std::string_view category,
                            std::string_view fallbackIconId);
std::string resolve_marker_id(std::string_view category, uint16_t team);
std::string resolve_alert_style_id(std::string_view category, std::string_view severity);
const char* glyph_for_icon(std::string_view iconId);

} // namespace dom::ui::icons
