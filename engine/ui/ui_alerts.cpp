#include "engine/ui/ui_alerts.h"

#include "engine/ui/ui_icons.h"

#include <algorithm>

namespace dom::ui::alerts {
namespace {

Severity classify_event(const dom::sim::MissionMessageRuntime& m, const dom::sim::World& world) {
  if (world.armageddonActive || m.styleTag == "armageddon") return Severity::Apocalyptic;
  if (m.priority >= 80 || m.category == "strategic" || m.category == "nuclear") return Severity::Critical;
  if (m.priority >= 40 || m.category == "warning" || m.category == "guardian" || m.category == "crisis") return Severity::Warning;
  return Severity::Info;
}

const char* severity_id(Severity s) {
  switch (s) {
    case Severity::Info: return "info";
    case Severity::Warning: return "warning";
    case Severity::Critical: return "critical";
    case Severity::Apocalyptic: return "apocalyptic";
  }
  return "info";
}

} // namespace

std::vector<StrategicAlert> build_alert_queue(const dom::sim::World& world, const UiState& uiState) {
  std::vector<StrategicAlert> alerts;
  alerts.reserve(world.missionMessages.size() + uiState.notifications.size() + 4);

  for (const auto& msg : world.missionMessages) {
    StrategicAlert a{};
    a.orderKey = (static_cast<uint64_t>(msg.tick) << 20ull) | (msg.sequence & 0xfffffull);
    a.severity = classify_event(msg, world);
    a.iconId = icons::resolve_icon_id(world, 0, msg.iconId, msg.category.empty() ? "event" : msg.category, "ui_icon_event");
    a.title = msg.title.empty() ? msg.category : msg.title;
    a.subtitle = msg.body;
    a.speaker = msg.speaker;
    a.emblemIconId = icons::civ_emblem_icon_id(world, 0);
    a.styleId = icons::resolve_alert_style_id(msg.category, severity_id(a.severity));
    alerts.push_back(std::move(a));
  }

  if (world.worldTension >= 0.85f) {
    StrategicAlert a{};
    a.orderKey = (static_cast<uint64_t>(world.tick) << 20ull) | 1ull;
    a.severity = world.worldTension >= 0.95f ? Severity::Critical : Severity::Warning;
    a.iconId = icons::resolve_icon_id(world, 0, "", "warning", "ui_icon_warning_strategic");
    a.title = "Global tension escalating";
    a.subtitle = "Strategic threshold nearing";
    a.emblemIconId = icons::civ_emblem_icon_id(world, 0);
    a.styleId = icons::resolve_alert_style_id("warning", severity_id(a.severity));
    alerts.push_back(std::move(a));
  }

  if (world.armageddonActive) {
    StrategicAlert a{};
    a.orderKey = (static_cast<uint64_t>(world.tick) << 20ull) | 2ull;
    a.severity = Severity::Apocalyptic;
    a.iconId = "ui_icon_warning_armageddon";
    a.title = "Armageddon active";
    a.subtitle = "Civilization-ending escalation in progress";
    a.emblemIconId = icons::civ_emblem_icon_id(world, 0);
    a.styleId = icons::resolve_alert_style_id("warning", "apocalyptic");
    alerts.push_back(std::move(a));
  }

  for (const auto& n : uiState.notifications) {
    StrategicAlert a{};
    a.orderKey = (static_cast<uint64_t>(n.expireTick) << 20ull) | 3ull;
    a.severity = Severity::Info;
    a.iconId = "ui_icon_event";
    a.title = "Strategic update";
    a.subtitle = n.text;
    a.emblemIconId = icons::civ_emblem_icon_id(world, 0);
    a.styleId = icons::resolve_alert_style_id("event", "info");
    alerts.push_back(std::move(a));
  }

  std::stable_sort(alerts.begin(), alerts.end(), [](const StrategicAlert& a, const StrategicAlert& b) {
    if (a.severity != b.severity) return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    return a.orderKey > b.orderKey;
  });
  if (alerts.size() > 8) alerts.resize(8);
  return alerts;
}

const char* severity_name(Severity s) {
  switch (s) {
    case Severity::Info: return "info";
    case Severity::Warning: return "warning";
    case Severity::Critical: return "critical";
    case Severity::Apocalyptic: return "apocalyptic";
  }
  return "info";
}

} // namespace dom::ui::alerts
