#pragma once

#include "engine/render/gltf_runtime_loader.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace dom::render {

enum class AnimationPlaybackHint : uint8_t { Loop, OneShot };

struct AnimationStyleBinding {
  std::string defaultState{"idle"};
  std::string defaultClip;
  std::unordered_map<std::string, std::string> stateClips;
  std::unordered_map<std::string, AnimationPlaybackHint> playbackHints;
};

struct RuntimeAnimationRequest {
  uint64_t stableId{0};
  uint64_t presentationTick{0};
  std::string requestedState{"idle"};
  const RuntimeModelData* model{nullptr};
  const AnimationStyleBinding* styleBinding{nullptr};
};

struct RuntimeAnimationState {
  bool animated{false};
  bool fallback{false};
  bool looping{false};
  bool playEvent{false};
  bool stateFallback{false};
  std::string resolvedState;
  std::string resolvedClip;
  float normalizedTime{0.0f};
};

struct RuntimeAnimationCounters {
  uint64_t animationResolveCount{0};
  uint64_t animationFallbackCount{0};
  uint64_t activeAnimatedInstances{0};
  uint64_t clipPlayEvents{0};
  uint64_t loopingClipInstances{0};
};

void reset_runtime_animation_counters();
const RuntimeAnimationCounters& runtime_animation_counters();
RuntimeAnimationState resolve_runtime_animation(const RuntimeAnimationRequest& request);

} // namespace dom::render
