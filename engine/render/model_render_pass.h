#pragma once

#include "engine/render/content_resolution.h"
#include "engine/render/model_cache.h"
#include "engine/render/render_stylesheet.h"
#include "engine/render/runtime_animation.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/vec2.hpp>

namespace dom::render {

struct ModelRenderCounters {
  uint64_t modelResolveCount{0};
  uint64_t modelFallbackCount{0};
  uint64_t activeModelInstances{0};
  uint64_t lodNearInstances{0};
  uint64_t lodMidInstances{0};
  uint64_t lodFarInstances{0};
  uint64_t attachmentResolveCount{0};
  uint64_t attachmentFallbackCount{0};
  uint64_t activeAttachmentInstances{0};
  uint64_t animationResolveCount{0};
  uint64_t animationFallbackCount{0};
  uint64_t activeAnimatedInstances{0};
  uint64_t clipPlayEvents{0};
  uint64_t loopingClipInstances{0};
  uint64_t terrainAwareInstances{0};
  uint64_t civTintInstances{0};
  uint64_t emissiveAccentInstances{0};
  uint64_t warningHighlightInstances{0};
  uint64_t industrialHighlightInstances{0};
  uint64_t guardianHighlightInstances{0};
  uint64_t damagedContrastInstances{0};
  uint64_t farReadabilityBoostInstances{0};
};

enum class ModelAttachmentSemantic : uint8_t {
  BannerSocket,
  CivEmblem,
  SmokeStack,
  MuzzleFlash,
  SelectionBadge,
  WarningBadge,
  GuardianAura,
};

struct ModelInstanceDesc {
  glm::vec2 pos{};
  float footprint{0.75f};
  std::array<float, 3> tint{1.0f, 1.0f, 1.0f};
  std::array<float, 3> terrainColor{0.24f, 0.26f, 0.24f};
  std::array<float, 3> terrainAccent{0.34f, 0.36f, 0.34f};
  MaterialReadabilityProfile readability{};
  float terrainAmbient{0.55f};
  float terrainDirectional{0.52f};
  float terrainContrast{0.10f};
  bool terrainAware{false};
  std::string meshId;
  std::string lodGroup;
  ContentLodTier lodTier{ContentLodTier::Near};
  bool selected{false};
  bool damaged{false};
  bool strategicWarning{false};
  bool activeIndustry{false};
  bool combatFiring{false};
  bool guardianActive{false};
  bool guardianRevealed{false};
  std::unordered_map<std::string, std::string> attachmentHooks;
  AnimationStyleBinding animation;
  std::string animationState{"idle"};
  uint64_t stableId{0};
  uint64_t presentationTick{0};
};

void reset_model_render_counters();
const ModelRenderCounters& model_render_counters();
void draw_model_instance(const ModelInstanceDesc& instance);

} // namespace dom::render
