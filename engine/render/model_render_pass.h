#pragma once

#include "engine/render/content_resolution.h"
#include "engine/render/model_cache.h"

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
};

void reset_model_render_counters();
const ModelRenderCounters& model_render_counters();
void draw_model_instance(const ModelInstanceDesc& instance);

} // namespace dom::render
