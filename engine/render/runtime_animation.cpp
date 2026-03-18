#include "engine/render/runtime_animation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_set>

namespace dom::render {
namespace {
RuntimeAnimationCounters gCounters{};
std::unordered_set<std::string> gWarnedMissing{};
std::unordered_set<std::string> gWarnedUnsupported{};

AnimationPlaybackHint playback_hint_for(const AnimationStyleBinding* binding,
                                        std::string_view state,
                                        std::string_view clip) {
  if (!binding) return AnimationPlaybackHint::Loop;
  if (auto it = binding->playbackHints.find(std::string(state)); it != binding->playbackHints.end()) return it->second;
  if (auto it = binding->playbackHints.find(std::string(clip)); it != binding->playbackHints.end()) return it->second;
  return AnimationPlaybackHint::Loop;
}

bool clip_available(const RuntimeModelData* model, std::string_view clipName) {
  if (!model || clipName.empty()) return false;
  return std::find(model->clipNames.begin(), model->clipNames.end(), clipName) != model->clipNames.end();
}

float deterministic_clip_duration(std::string_view clipName) {
  uint32_t hash = 2166136261u;
  for (char c : clipName) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  return 18.0f + static_cast<float>(hash % 45u);
}

std::string first_available_clip(const RuntimeAnimationRequest& request,
                                 const AnimationStyleBinding* binding,
                                 std::string_view resolvedState,
                                 bool& outStateFallback) {
  outStateFallback = false;

  if (!binding) return {};

  if (auto it = binding->stateClips.find(std::string(resolvedState)); it != binding->stateClips.end() && !it->second.empty()) {
    return it->second;
  }

  if (!binding->defaultState.empty() && binding->defaultState != resolvedState) {
    if (auto it = binding->stateClips.find(binding->defaultState); it != binding->stateClips.end() && !it->second.empty()) {
      outStateFallback = true;
      return it->second;
    }
  }

  if (!binding->defaultClip.empty()) return binding->defaultClip;
  if (clip_available(request.model, resolvedState)) return std::string(resolvedState);
  return {};
}

} // namespace

void reset_runtime_animation_counters() { gCounters = {}; }

const RuntimeAnimationCounters& runtime_animation_counters() { return gCounters; }

RuntimeAnimationState resolve_runtime_animation(const RuntimeAnimationRequest& request) {
  ++gCounters.animationResolveCount;

  RuntimeAnimationState state{};
  state.resolvedState = request.requestedState.empty() ? "idle" : request.requestedState;

  if (!request.model || request.model->clipNames.empty()) {
    state.fallback = true;
    ++gCounters.animationFallbackCount;
    return state;
  }

  const AnimationStyleBinding* binding = request.styleBinding;
  bool stateFallback = false;
  state.resolvedClip = first_available_clip(request, binding, state.resolvedState, stateFallback);
  state.stateFallback = stateFallback;

  if (!state.resolvedClip.empty() && !clip_available(request.model, state.resolvedClip)) {
#ifndef NDEBUG
    const std::string warnKey = request.model->sourcePath + "|" + state.resolvedState + "|" + state.resolvedClip;
    if (!gWarnedMissing.contains(warnKey)) {
      gWarnedMissing.insert(warnKey);
      std::cerr << "[render][anim] missing clip '" << state.resolvedClip << "' for model '" << request.model->sourcePath
                << "' state='" << state.resolvedState << "', falling back\n";
    }
#endif
    state.resolvedClip.clear();
    state.stateFallback = true;
  }

  if (state.resolvedClip.empty()) {
    state.resolvedClip = request.model->clipNames.front();
    state.fallback = true;
    ++gCounters.animationFallbackCount;
  }

  if (state.resolvedClip.empty()) {
#ifndef NDEBUG
    if (!gWarnedUnsupported.contains(request.model->sourcePath)) {
      gWarnedUnsupported.insert(request.model->sourcePath);
      std::cerr << "[render][anim] unsupported animation payload for model '" << request.model->sourcePath << "'\n";
    }
#endif
    state.fallback = true;
    ++gCounters.animationFallbackCount;
    return state;
  }

  state.animated = true;
  ++gCounters.activeAnimatedInstances;

  const auto hint = playback_hint_for(binding, state.resolvedState, state.resolvedClip);
  state.looping = hint == AnimationPlaybackHint::Loop;
  if (state.looping) ++gCounters.loopingClipInstances;

  const float duration = deterministic_clip_duration(state.resolvedClip);
  const float t = static_cast<float>((request.presentationTick + request.stableId) % 8192u);
  const float phase = std::fmod(t / duration, 1.0f);
  state.normalizedTime = std::clamp(phase, 0.0f, 1.0f);

  if (!state.looping && state.normalizedTime < 0.06f) {
    state.playEvent = true;
    ++gCounters.clipPlayEvents;
  }

  return state;
}

} // namespace dom::render
