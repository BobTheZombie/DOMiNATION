#pragma once

#include "engine/audio/audio_events.h"
#include "engine/audio/audio_resolution.h"
#include "engine/sim/simulation.h"
#include <array>
#include <string>
#include <vector>

namespace dom::audio {

struct AudioSettings {
  bool enabled{true};
  float masterVolume{0.65f};
  float uiVolume{0.9f};
  float worldVolume{0.8f};
  float ambientVolume{0.5f};
};

struct AudioDebugCounters {
  uint64_t audioResolveCount{0};
  uint64_t audioFallbackCount{0};
  uint64_t activeAmbientChannels{0};
  uint64_t eventSoundTriggers{0};
  uint64_t uiSoundTriggers{0};
};

bool initialize(bool allowDevice, bool devMode);
void shutdown();
void set_settings(const AudioSettings& settings);
AudioSettings settings();
AudioDebugCounters debug_counters();
std::vector<std::string> consume_debug_reports();

void trigger_ui(AudioEventKey key, uint32_t tick, uint32_t stableId, const std::string& civilization);
void trigger_world(AudioEventKey key, uint32_t tick, uint32_t stableId, const std::string& civilization);
void trigger_from_gameplay_event(const dom::sim::World& world, const dom::sim::GameplayEvent& ev);
void update_ambient(const dom::sim::World& world, float cameraZoom);

} // namespace dom::audio
