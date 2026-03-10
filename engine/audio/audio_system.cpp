#include "engine/audio/audio_system.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dom::audio {
namespace {

struct AudioRuntime {
  AudioSettings settings{};
  AudioResolver resolver{};
  AudioDebugCounters counters{};
  bool initialized{false};
  bool devMode{false};
  SDL_AudioDeviceID device{0};
  SDL_AudioSpec spec{};
  uint32_t lastAmbientTick{0};
  bool armageddonPlayed{false};
  uint32_t lastStrategicStrikeEvents{0};
  uint32_t lastInterceptionEvents{0};
  uint32_t lastTriggeredWorldEventCount{0};
  uint32_t lastResolvedWorldEventCount{0};
  uint32_t lastGuardiansSpawned{0};
  std::vector<std::string> debugReports;
};

AudioRuntime gAudio;

float event_volume(AudioCategory cat) {
  float group = gAudio.settings.worldVolume;
  if (cat == AudioCategory::UI || cat == AudioCategory::UnitCommand) group = gAudio.settings.uiVolume;
  if (cat == AudioCategory::Ambient) group = gAudio.settings.ambientVolume;
  return std::clamp(gAudio.settings.masterVolume * group, 0.0f, 1.0f);
}

uint32_t hash32(const std::string& s, uint32_t salt) {
  uint32_t h = 2166136261u ^ salt;
  for (char c : s) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
  return h;
}

void queue_tone(const std::string& soundId, float volume, uint32_t stableId) {
  if (gAudio.device == 0 || volume <= 0.0f) return;
  const uint32_t h = hash32(soundId, stableId);
  const int sampleRate = gAudio.spec.freq > 0 ? gAudio.spec.freq : 48000;
  const int ms = 45 + static_cast<int>(h % 90u);
  const int sampleCount = std::max(64, sampleRate * ms / 1000);
  const float freq = 180.0f + static_cast<float>((h % 900u));
  std::vector<float> pcm(sampleCount, 0.0f);
  const float amp = volume * 0.18f;
  for (int i = 0; i < sampleCount; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(sampleRate);
    float env = std::max(0.0f, 1.0f - static_cast<float>(i) / static_cast<float>(sampleCount));
    pcm[i] = std::sin(2.0f * 3.14159265f * freq * t) * amp * env;
  }
  SDL_QueueAudio(gAudio.device, pcm.data(), static_cast<Uint32>(pcm.size() * sizeof(float)));
}

void trigger_impl(AudioEventKey key, uint32_t tick, uint32_t stableId, const std::string& civ, bool ui) {
  if (!gAudio.initialized || !gAudio.settings.enabled) return;
  auto resolved = gAudio.resolver.resolve(key, civ, civ);
  const auto& rc = gAudio.resolver.counters();
  gAudio.counters.audioResolveCount = rc.audioResolveCount;
  gAudio.counters.audioFallbackCount = rc.audioFallbackCount;
  if (resolved.silent) return;
  if (ui) ++gAudio.counters.uiSoundTriggers;
  else ++gAudio.counters.eventSoundTriggers;
  queue_tone(resolved.soundId, event_volume(resolved.category), stableId ^ tick);
}

} // namespace

bool initialize(bool allowDevice, bool devMode) {
  gAudio = {};
  gAudio.devMode = devMode;
  gAudio.resolver.load_manifest("content/audio_manifest.json");
  if (allowDevice) {
    SDL_AudioSpec desired{};
    desired.freq = 48000;
    desired.format = AUDIO_F32SYS;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = nullptr;
    gAudio.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &gAudio.spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (gAudio.device != 0) SDL_PauseAudioDevice(gAudio.device, 0);
  }
  gAudio.initialized = true;
  return true;
}

void shutdown() {
  if (gAudio.device != 0) SDL_CloseAudioDevice(gAudio.device);
  gAudio = {};
}

void set_settings(const AudioSettings& settings) { gAudio.settings = settings; }
AudioSettings settings() { return gAudio.settings; }
AudioDebugCounters debug_counters() { return gAudio.counters; }

std::vector<std::string> consume_debug_reports() {
  auto out = gAudio.resolver.consume_missing_reports();
  return out;
}

void trigger_ui(AudioEventKey key, uint32_t tick, uint32_t stableId, const std::string& civilization) {
  trigger_impl(key, tick, stableId, civilization, true);
}

void trigger_world(AudioEventKey key, uint32_t tick, uint32_t stableId, const std::string& civilization) {
  trigger_impl(key, tick, stableId, civilization, false);
}

void trigger_from_gameplay_event(const dom::sim::World& world, const dom::sim::GameplayEvent& ev) {
  AudioEventKey k = AudioEventKey::CombatSmallArms;
  bool emit = true;
  switch (ev.type) {
    case dom::sim::GameplayEventType::UnitDied: k = AudioEventKey::CombatArmorImpact; break;
    case dom::sim::GameplayEventType::ObjectiveCompleted: k = AudioEventKey::ObjectiveComplete; break;
    case dom::sim::GameplayEventType::GuardianDiscovered: k = AudioEventKey::GuardianDiscovered; break;
    case dom::sim::GameplayEventType::GuardianJoined: k = AudioEventKey::GuardianJoined; break;
    case dom::sim::GameplayEventType::WarDeclared: k = AudioEventKey::CrisisActivate; break;
    default: emit = false; break;
  }
  if (!emit) return;
  trigger_world(k, ev.tick, ev.entityId ^ ev.actor ^ ev.subject, world.players.empty() ? "default" : world.players[0].civilization.id);
}

void update_ambient(const dom::sim::World& world, float cameraZoom) {
  if (!gAudio.settings.enabled || world.tick - gAudio.lastAmbientTick < 180) return;
  gAudio.lastAmbientTick = world.tick;
  gAudio.counters.activeAmbientChannels = 0;
  const std::string civ = world.players.empty() ? "default" : world.players[0].civilization.id;
  trigger_world(AudioEventKey::AmbientWind, world.tick, world.tick, civ);
  ++gAudio.counters.activeAmbientChannels;
  if (cameraZoom < 16.0f && !world.coastClassMap.empty()) { trigger_world(AudioEventKey::AmbientCoast, world.tick, world.tick + 1, civ); ++gAudio.counters.activeAmbientChannels; }
  if (cameraZoom < 14.0f && !world.coastClassMap.empty()) { trigger_world(AudioEventKey::AmbientForest, world.tick, world.tick + 2, civ); ++gAudio.counters.activeAmbientChannels; }
  if (world.factoryCount > 0) { trigger_world(AudioEventKey::AmbientIndustry, world.tick, world.tick + 3, civ); ++gAudio.counters.activeAmbientChannels; }
  if (!world.trains.empty() || world.railEdgeCount > 0) { trigger_world(AudioEventKey::AmbientRail, world.tick, world.tick + 4, civ); ++gAudio.counters.activeAmbientChannels; }
  if (cameraZoom < 18.0f && world.combatEngagementCount > 0) { trigger_world(AudioEventKey::AmbientBattle, world.tick, world.tick + 5, civ); ++gAudio.counters.activeAmbientChannels; }
  if (world.strategicStrikeEvents > gAudio.lastStrategicStrikeEvents) {
    trigger_world(AudioEventKey::StrategicLaunchWarning, world.tick, world.strategicStrikeEvents, civ);
    gAudio.lastStrategicStrikeEvents = world.strategicStrikeEvents;
  }
  if (world.interceptionEvents > gAudio.lastInterceptionEvents) {
    trigger_world(AudioEventKey::StrategicInterception, world.tick, world.interceptionEvents, civ);
    gAudio.lastInterceptionEvents = world.interceptionEvents;
  }
  if (world.triggeredWorldEventCount > gAudio.lastTriggeredWorldEventCount) {
    trigger_world(AudioEventKey::CrisisActivate, world.tick, world.triggeredWorldEventCount, civ);
    gAudio.lastTriggeredWorldEventCount = world.triggeredWorldEventCount;
  }
  if (world.resolvedWorldEventCount > gAudio.lastResolvedWorldEventCount) {
    trigger_world(AudioEventKey::CrisisResolve, world.tick, world.resolvedWorldEventCount, civ);
    gAudio.lastResolvedWorldEventCount = world.resolvedWorldEventCount;
  }
  if (world.guardiansSpawned > gAudio.lastGuardiansSpawned) {
    trigger_world(AudioEventKey::GuardianAwaken, world.tick, world.guardiansSpawned, civ);
    gAudio.lastGuardiansSpawned = world.guardiansSpawned;
  }
  if (world.armageddonActive && !gAudio.armageddonPlayed) {
    trigger_world(AudioEventKey::ArmageddonAlarm, world.tick, world.armageddonTriggerTick, civ);
    gAudio.armageddonPlayed = true;
  }
}

} // namespace dom::audio
