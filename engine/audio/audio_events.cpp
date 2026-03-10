#include "engine/audio/audio_events.h"

namespace dom::audio {

const char* audio_event_key_name(AudioEventKey key) {
  switch (key) {
    case AudioEventKey::UiButtonSelect: return "ui_select";
    case AudioEventKey::UiButtonConfirm: return "ui_confirm";
    case AudioEventKey::UiButtonCancel: return "ui_cancel";
    case AudioEventKey::UiPanelOpen: return "ui_panel_open";
    case AudioEventKey::UiPanelClose: return "ui_panel_close";
    case AudioEventKey::SelectionAcknowledge: return "selection_ack";
    case AudioEventKey::CommandMove: return "command_move";
    case AudioEventKey::CommandAttack: return "command_attack";
    case AudioEventKey::CommandBuild: return "command_build";
    case AudioEventKey::ObjectiveComplete: return "objective_complete";
    case AudioEventKey::ObjectiveFail: return "objective_fail";
    case AudioEventKey::CombatSmallArms: return "combat_small_arms";
    case AudioEventKey::CombatArtillery: return "combat_artillery";
    case AudioEventKey::CombatArmorImpact: return "combat_armor_impact";
    case AudioEventKey::CombatNavalGunfire: return "combat_naval_gunfire";
    case AudioEventKey::CombatAirStrike: return "combat_air_strike";
    case AudioEventKey::StrategicLaunchWarning: return "strategic_launch_warning";
    case AudioEventKey::StrategicInterception: return "strategic_interception";
    case AudioEventKey::StrategicDetonation: return "strategic_detonation";
    case AudioEventKey::ArmageddonAlarm: return "armageddon_alarm";
    case AudioEventKey::CrisisActivate: return "crisis_activate";
    case AudioEventKey::CrisisResolve: return "crisis_resolve";
    case AudioEventKey::GuardianDiscovered: return "guardian_discovered";
    case AudioEventKey::GuardianAwaken: return "guardian_awaken";
    case AudioEventKey::GuardianJoined: return "guardian_joined";
    case AudioEventKey::AmbientWind: return "ambient_wind";
    case AudioEventKey::AmbientCoast: return "ambient_coast";
    case AudioEventKey::AmbientForest: return "ambient_forest";
    case AudioEventKey::AmbientIndustry: return "ambient_industry";
    case AudioEventKey::AmbientRail: return "ambient_rail";
    case AudioEventKey::AmbientBattle: return "ambient_battle";
  }
  return "ui_select";
}

const char* audio_category_name(AudioCategory category) {
  switch (category) {
    case AudioCategory::UI: return "ui";
    case AudioCategory::UnitCommand: return "unit_command";
    case AudioCategory::Combat: return "combat";
    case AudioCategory::Strategic: return "strategic";
    case AudioCategory::Crisis: return "crisis";
    case AudioCategory::Guardian: return "guardian";
    case AudioCategory::Ambient: return "ambient";
    case AudioCategory::IndustrialLogistics: return "industrial_logistics";
  }
  return "ui";
}

} // namespace dom::audio
