#include "engine/ui/ui_icons.h"
#include "engine/render/content_resolution.h"

#include <array>
#include <unordered_map>

namespace dom::ui::icons {
namespace {
PresentationCounters gCounters{};

const std::unordered_map<std::string, std::string> kCivEmblemById = {
    {"rome", "ui_emblem_rome"}, {"china", "ui_emblem_china"}, {"europe", "ui_emblem_europe"},
    {"middle_east", "ui_emblem_middle_east"}, {"russia", "ui_emblem_russia"}, {"usa", "ui_emblem_usa"},
    {"japan", "ui_emblem_japan"}, {"eu", "ui_emblem_eu"}, {"uk", "ui_emblem_uk"},
    {"egypt", "ui_emblem_egypt"}, {"tartaria", "ui_emblem_tartaria"}};

const std::unordered_map<std::string, std::string> kCategoryIcon = {
    {"resource", "ui_icon_resource_food"}, {"refined", "ui_icon_refined_steel"},
    {"unit", "ui_icon_unit_infantry"}, {"building", "ui_icon_building_city_center"},
    {"diplomacy", "ui_icon_diplomacy"}, {"objective", "ui_icon_objective"},
    {"event", "ui_icon_event"}, {"warning", "ui_icon_warning_strategic"},
    {"guardian", "ui_icon_event_guardian"}, {"research", "ui_icon_research"},
    {"campaign", "ui_icon_campaign"}, {"command", "ui_icon_command"}};

const std::unordered_map<std::string, const char*> kGlyphByPrefix = {
    {"ui_icon_resource", "◈"}, {"ui_icon_refined", "◉"}, {"ui_icon_unit", "▲"},
    {"ui_icon_building", "▣"}, {"ui_emblem", "⬢"}, {"ui_icon_warning", "⚠"},
    {"ui_icon_event", "✦"}, {"ui_icon_objective", "◆"}, {"ui_icon_guardian", "☬"},
    {"ui_icon_campaign", "✪"}, {"ui_icon_command", "➤"}};

bool is_fallback(std::string_view id) {
  return id.empty() || id.find("fallback") != std::string_view::npos;
}

} // namespace

void reset_frame_counters() { gCounters = {}; }

const PresentationCounters& presentation_counters() { return gCounters; }

std::string civ_emblem_icon_id(const dom::sim::World& world, uint16_t team) {
  ++gCounters.iconResolveCount;
  if (team < world.players.size()) {
    const auto& civ = world.players[team].civilization;
    auto it = kCivEmblemById.find(civ.id);
    if (it != kCivEmblemById.end()) return it->second;
    it = kCivEmblemById.find(civ.themeId);
    if (it != kCivEmblemById.end()) return it->second;
  }
  ++gCounters.presentationFallbackCount;
  return "ui_emblem_default";
}

std::string resolve_icon_id(const dom::sim::World& world,
                            uint16_t team,
                            const std::string& exactIconId,
                            std::string_view category,
                            std::string_view fallbackIconId) {
  ++gCounters.iconResolveCount;
  std::string civMapping;
  if (category == "civ") civMapping = civ_emblem_icon_id(world, team);
  std::string categoryMapping;
  auto cat = kCategoryIcon.find(std::string(category));
  if (cat != kCategoryIcon.end()) categoryMapping = cat->second;

  const std::string resolved = dom::render::resolve_content_id(
      exactIconId,
      civMapping,
      {},
      categoryMapping,
      fallbackIconId,
      dom::render::ContentResolutionDomain::Icon);
  if (is_fallback(resolved)) ++gCounters.presentationFallbackCount;
  return resolved;
}

std::string resolve_marker_id(std::string_view category, uint16_t team) {
  (void)team;
  ++gCounters.markerResolveCount;
  if (category == "capital") return "ui_marker_capital";
  if (category == "port") return "ui_marker_port";
  if (category == "rail") return "ui_marker_rail_hub";
  if (category == "factory") return "ui_marker_factory";
  if (category == "radar") return "ui_marker_radar";
  if (category == "missile") return "ui_marker_missile";
  if (category == "guardian") return "ui_marker_guardian";
  if (category == "warning") return "ui_marker_warning";
  ++gCounters.presentationFallbackCount;
  return "ui_marker_generic";
}

std::string resolve_alert_style_id(std::string_view category, std::string_view severity) {
  ++gCounters.alertResolveCount;
  if (severity == "apocalyptic") return "ui_alert_armageddon";
  if (severity == "critical") return "ui_alert_critical";
  if (severity == "warning") return "ui_alert_warning";
  if (category == "objective") return "ui_alert_objective";
  if (category == "guardian") return "ui_alert_guardian";
  return "ui_alert_info";
}

const char* glyph_for_icon(std::string_view iconId) {
  for (const auto& [prefix, glyph] : kGlyphByPrefix) {
    if (iconId.find(prefix) == 0) return glyph;
  }
  return "•";
}

} // namespace dom::ui::icons
