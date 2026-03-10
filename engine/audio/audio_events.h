#pragma once

#include <cstdint>

namespace dom::audio {

enum class AudioCategory : uint8_t {
  UI,
  UnitCommand,
  Combat,
  Strategic,
  Crisis,
  Guardian,
  Ambient,
  IndustrialLogistics
};

enum class AudioEventKey : uint16_t {
  UiButtonSelect,
  UiButtonConfirm,
  UiButtonCancel,
  UiPanelOpen,
  UiPanelClose,
  SelectionAcknowledge,
  CommandMove,
  CommandAttack,
  CommandBuild,
  ObjectiveComplete,
  ObjectiveFail,
  CombatSmallArms,
  CombatArtillery,
  CombatArmorImpact,
  CombatNavalGunfire,
  CombatAirStrike,
  StrategicLaunchWarning,
  StrategicInterception,
  StrategicDetonation,
  ArmageddonAlarm,
  CrisisActivate,
  CrisisResolve,
  GuardianDiscovered,
  GuardianAwaken,
  GuardianJoined,
  AmbientWind,
  AmbientCoast,
  AmbientForest,
  AmbientIndustry,
  AmbientRail,
  AmbientBattle,
};

const char* audio_event_key_name(AudioEventKey key);
const char* audio_category_name(AudioCategory category);

} // namespace dom::audio
