#include "engine/audio/audio_resolution.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace dom::audio {
namespace {

AudioCategory parse_category(const std::string& s) {
  if (s == "unit_command") return AudioCategory::UnitCommand;
  if (s == "combat") return AudioCategory::Combat;
  if (s == "strategic") return AudioCategory::Strategic;
  if (s == "crisis") return AudioCategory::Crisis;
  if (s == "guardian") return AudioCategory::Guardian;
  if (s == "ambient") return AudioCategory::Ambient;
  if (s == "industrial_logistics") return AudioCategory::IndustrialLogistics;
  return AudioCategory::UI;
}

} // namespace

AudioCategory category_for_event(AudioEventKey key) {
  switch (key) {
    case AudioEventKey::UiButtonSelect:
    case AudioEventKey::UiButtonConfirm:
    case AudioEventKey::UiButtonCancel:
    case AudioEventKey::UiPanelOpen:
    case AudioEventKey::UiPanelClose:
    case AudioEventKey::ObjectiveComplete:
    case AudioEventKey::ObjectiveFail:
      return AudioCategory::UI;
    case AudioEventKey::SelectionAcknowledge:
    case AudioEventKey::CommandMove:
    case AudioEventKey::CommandAttack:
    case AudioEventKey::CommandBuild:
      return AudioCategory::UnitCommand;
    case AudioEventKey::CombatSmallArms:
    case AudioEventKey::CombatArtillery:
    case AudioEventKey::CombatArmorImpact:
    case AudioEventKey::CombatNavalGunfire:
    case AudioEventKey::CombatAirStrike:
      return AudioCategory::Combat;
    case AudioEventKey::StrategicLaunchWarning:
    case AudioEventKey::StrategicInterception:
    case AudioEventKey::StrategicDetonation:
    case AudioEventKey::ArmageddonAlarm:
      return AudioCategory::Strategic;
    case AudioEventKey::CrisisActivate:
    case AudioEventKey::CrisisResolve:
      return AudioCategory::Crisis;
    case AudioEventKey::GuardianDiscovered:
    case AudioEventKey::GuardianAwaken:
    case AudioEventKey::GuardianJoined:
      return AudioCategory::Guardian;
    case AudioEventKey::AmbientWind:
    case AudioEventKey::AmbientCoast:
    case AudioEventKey::AmbientForest:
    case AudioEventKey::AmbientIndustry:
    case AudioEventKey::AmbientRail:
    case AudioEventKey::AmbientBattle:
      return AudioCategory::Ambient;
  }
  return AudioCategory::UI;
}

bool AudioResolver::load_manifest(const std::string& path) {
  std::ifstream in(path);
  if (!in.good()) return false;
  nlohmann::json j; in >> j;
  auto load_map = [&](const char* key, auto& out) {
    if (!j.contains(key) || !j[key].is_object()) return;
    for (auto it = j[key].begin(); it != j[key].end(); ++it) {
      Entry e{};
      if (it.value().is_string()) e.soundId = it.value().get<std::string>();
      else {
        e.soundId = it.value().value("sound", std::string(""));
        e.category = parse_category(it.value().value("category", std::string("ui")));
      }
      if (!e.soundId.empty()) out[it.key()] = e;
    }
  };
  load_map("exact", exact_);
  load_map("civilization", civ_);
  load_map("category", category_);
  load_map("default", defaults_);
  return true;
}

AudioResolveResult AudioResolver::resolve(AudioEventKey key, const std::string& civilization, const std::string& theme) {
  ++counters_.audioResolveCount;
  const std::string eventKey = audio_event_key_name(key);
  const AudioCategory category = category_for_event(key);
  const std::string categoryName = audio_category_name(category);

  auto from = [&](const auto& map, const std::string& k, bool fb) -> AudioResolveResult {
    auto it = map.find(k);
    if (it == map.end()) return {};
    if (fb) ++counters_.audioFallbackCount;
    return {it->second.soundId, it->second.category, fb, false};
  };

  if (auto r = from(exact_, eventKey, false); !r.soundId.empty()) return r;
  if (!civilization.empty()) {
    if (auto r = from(civ_, civilization + "." + eventKey, true); !r.soundId.empty()) return r;
  }
  if (!theme.empty()) {
    if (auto r = from(civ_, theme + "." + eventKey, true); !r.soundId.empty()) return r;
  }
  if (auto r = from(category_, categoryName, true); !r.soundId.empty()) return r;
  if (auto r = from(defaults_, "default", true); !r.soundId.empty()) return r;

  ++counters_.audioFallbackCount;
  missingReports_.push_back("AUDIO_MISSING event=" + eventKey + " category=" + categoryName);
  return {"", category, true, true};
}

std::vector<std::string> AudioResolver::consume_missing_reports() {
  auto out = missingReports_;
  missingReports_.clear();
  return out;
}

} // namespace dom::audio
