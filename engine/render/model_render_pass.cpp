#include "engine/render/model_render_pass.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

namespace dom::render {
namespace {
ModelCache gModelCache{};
ModelRenderCounters gModelCounters{};

struct AttachmentVisualState {
  bool enabled{false};
  float intensity{0.0f};
  float sizeScale{1.0f};
  float heightBias{0.0f};
  float alpha{1.0f};
};

float lod_scale(ContentLodTier tier) {
  switch (tier) {
    case ContentLodTier::Near: return 1.0f;
    case ContentLodTier::Mid: return 0.8f;
    case ContentLodTier::Far: return 0.62f;
  }
  return 1.0f;
}

float stable_phase(const ModelInstanceDesc& instance, float scale) {
  const float t = static_cast<float>((instance.presentationTick + instance.stableId * 17ull) % 4096u);
  return std::sin(t * scale + instance.pos.x * 0.37f + instance.pos.y * 0.21f);
}

AttachmentVisualState attachment_visual_state(ModelAttachmentSemantic semantic,
                                              const ModelInstanceDesc& instance,
                                              const RuntimeAnimationState& animation) {
  AttachmentVisualState out{};
  const float pulse = stable_phase(instance, 0.071f) * 0.5f + 0.5f;
  const float animPulse = animation.animated ? (0.45f + 0.55f * std::sin(animation.normalizedTime * 6.2831853f)) : pulse;

  switch (semantic) {
    case ModelAttachmentSemantic::BannerSocket:
      out.enabled = true;
      out.intensity = 0.58f + pulse * 0.32f;
      out.sizeScale = 1.08f + pulse * 0.16f;
      out.heightBias = 0.10f;
      out.alpha = 0.92f;
      break;
    case ModelAttachmentSemantic::CivEmblem:
      out.enabled = true;
      out.intensity = 0.75f + pulse * 0.18f;
      out.sizeScale = 0.92f + animPulse * 0.16f;
      out.heightBias = 0.06f;
      out.alpha = 0.96f;
      break;
    case ModelAttachmentSemantic::SmokeStack:
      out.enabled = instance.activeIndustry;
      out.intensity = 0.65f + animPulse * 0.35f;
      out.sizeScale = 1.0f + animPulse * 0.28f;
      out.heightBias = 0.24f + animPulse * 0.12f;
      out.alpha = 0.42f + animPulse * 0.16f;
      break;
    case ModelAttachmentSemantic::MuzzleFlash:
      out.enabled = instance.combatFiring;
      out.intensity = animation.playEvent ? 1.0f : (animation.animated ? std::max(0.0f, 1.0f - animation.normalizedTime) : 0.78f);
      out.sizeScale = 1.0f + out.intensity * 0.65f;
      out.heightBias = 0.04f;
      out.alpha = 0.85f + out.intensity * 0.15f;
      break;
    case ModelAttachmentSemantic::SelectionBadge:
      out.enabled = instance.selected;
      out.intensity = 0.86f + pulse * 0.14f;
      out.sizeScale = 1.05f + pulse * 0.18f;
      out.heightBias = -0.10f;
      out.alpha = 0.98f;
      break;
    case ModelAttachmentSemantic::WarningBadge:
      out.enabled = instance.strategicWarning || instance.damaged;
      out.intensity = instance.strategicWarning ? (0.86f + pulse * 0.14f) : (0.68f + pulse * 0.18f);
      out.sizeScale = 1.08f + out.intensity * 0.18f;
      out.heightBias = 0.18f;
      out.alpha = 0.96f;
      break;
    case ModelAttachmentSemantic::GuardianAura:
      out.enabled = instance.guardianActive || instance.guardianRevealed;
      out.intensity = instance.guardianActive ? (0.82f + animPulse * 0.18f) : (0.52f + pulse * 0.16f);
      out.sizeScale = instance.guardianActive ? (1.2f + animPulse * 0.28f) : (1.02f + pulse * 0.14f);
      out.heightBias = -0.12f;
      out.alpha = instance.guardianActive ? 0.9f : 0.58f;
      break;
  }
  return out;
}

std::array<float, 3> semantic_color(ModelAttachmentSemantic semantic, const std::array<float, 3>& baseTint, bool emphasis) {
  switch (semantic) {
    case ModelAttachmentSemantic::BannerSocket:
      return {std::min(1.0f, baseTint[0] + 0.16f), std::min(1.0f, baseTint[1] + 0.16f), std::min(1.0f, baseTint[2] + 0.16f)};
    case ModelAttachmentSemantic::CivEmblem:
      return {std::min(1.0f, baseTint[0] + 0.28f), std::min(1.0f, baseTint[1] + 0.26f), std::min(1.0f, baseTint[2] + 0.18f)};
    case ModelAttachmentSemantic::SmokeStack:
      return {0.34f, 0.34f, 0.36f};
    case ModelAttachmentSemantic::MuzzleFlash:
      return {1.0f, emphasis ? 0.94f : 0.84f, 0.22f};
    case ModelAttachmentSemantic::SelectionBadge:
      return {1.0f, 0.95f, 0.26f};
    case ModelAttachmentSemantic::WarningBadge:
      return {1.0f, 0.32f, 0.26f};
    case ModelAttachmentSemantic::GuardianAura:
      return emphasis ? std::array<float, 3>{0.95f, 0.42f, 0.32f} : std::array<float, 3>{0.72f, 0.34f, 0.95f};
  }
  return baseTint;
}

bool to_semantic(std::string_view name, ModelAttachmentSemantic& out) {
  if (name == "banner_socket") { out = ModelAttachmentSemantic::BannerSocket; return true; }
  if (name == "civ_emblem") { out = ModelAttachmentSemantic::CivEmblem; return true; }
  if (name == "smoke_stack") { out = ModelAttachmentSemantic::SmokeStack; return true; }
  if (name == "muzzle_flash") { out = ModelAttachmentSemantic::MuzzleFlash; return true; }
  if (name == "selection_badge") { out = ModelAttachmentSemantic::SelectionBadge; return true; }
  if (name == "warning_badge") { out = ModelAttachmentSemantic::WarningBadge; return true; }
  if (name == "guardian_aura") { out = ModelAttachmentSemantic::GuardianAura; return true; }
  return false;
}

void draw_attachment(const ModelInstanceDesc& instance,
                     float base,
                     float height,
                     ModelAttachmentSemantic semantic,
                     const AttachmentHookResolveResult& hook,
                     const RuntimeAnimationState& animation,
                     bool fallbackHook) {
  const auto visual = attachment_visual_state(semantic, instance, animation);
  if (!visual.enabled) return;

  ++gModelCounters.activeAttachmentInstances;

  const auto col = semantic_color(semantic, instance.tint, instance.guardianActive || instance.combatFiring || instance.strategicWarning);

  float ox = hook.normalizedOffset.x;
  float oy = hook.normalizedOffset.y;
  float oz = hook.normalizedOffset.z;
  if (fallbackHook || !hook.valid) {
    ox = 0.0f;
    oy = 0.0f;
    oz = 0.0f;
  }
  const float cx = instance.pos.x + ox * base;
  const float cy = instance.pos.y + oy * base + (oy * height * 0.22f) + oz * height * 0.18f + visual.heightBias * base;

  switch (semantic) {
    case ModelAttachmentSemantic::SmokeStack: {
      const float plume = base * (0.24f + visual.intensity * 0.28f);
      glColor4f(col[0], col[1], col[2], visual.alpha);
      glBegin(GL_TRIANGLES);
      glVertex2f(cx, cy + plume * 1.45f);
      glVertex2f(cx - plume * 0.72f, cy - plume * 0.28f);
      glVertex2f(cx + plume * 0.72f, cy - plume * 0.28f);
      glEnd();
      break;
    }
    case ModelAttachmentSemantic::GuardianAura: {
      const float radius = base * visual.sizeScale * (1.02f + visual.intensity * 0.32f);
      glBegin(GL_LINE_LOOP);
      glColor4f(col[0], col[1], col[2], visual.alpha);
      for (int i = 0; i < 20; ++i) {
        const float a = static_cast<float>(i) * 0.31415926f;
        const float wobble = 1.0f + 0.06f * std::sin(a * 3.0f + animation.normalizedTime * 6.2831853f);
        glVertex2f(cx + std::cos(a) * radius * wobble, cy + std::sin(a) * radius * wobble);
      }
      glEnd();
      break;
    }
    case ModelAttachmentSemantic::BannerSocket: {
      const float width = base * 0.16f * visual.sizeScale;
      const float heightSpan = base * 0.42f * visual.sizeScale;
      glColor4f(col[0], col[1], col[2], visual.alpha);
      glBegin(GL_QUADS);
      glVertex2f(cx - width * 0.15f, cy - heightSpan * 0.55f);
      glVertex2f(cx + width * 0.15f, cy - heightSpan * 0.55f);
      glVertex2f(cx + width * 0.15f, cy + heightSpan * 0.62f);
      glVertex2f(cx - width * 0.15f, cy + heightSpan * 0.62f);
      glEnd();
      glBegin(GL_TRIANGLES);
      glVertex2f(cx + width * 0.15f, cy + heightSpan * 0.3f);
      glVertex2f(cx + width * 1.1f, cy + heightSpan * 0.12f);
      glVertex2f(cx + width * 0.15f, cy - heightSpan * 0.02f);
      glEnd();
      break;
    }
    case ModelAttachmentSemantic::CivEmblem: {
      const float s = base * (0.14f + visual.intensity * 0.08f) * visual.sizeScale;
      glColor4f(col[0], col[1], col[2], visual.alpha);
      glBegin(GL_QUADS);
      glVertex2f(cx, cy + s);
      glVertex2f(cx + s, cy);
      glVertex2f(cx, cy - s);
      glVertex2f(cx - s, cy);
      glEnd();
      break;
    }
    default: {
      const float s = base * (semantic == ModelAttachmentSemantic::MuzzleFlash ? 0.18f + visual.intensity * 0.18f : 0.14f + visual.intensity * 0.08f) * visual.sizeScale;
      glColor4f(col[0], col[1], col[2], visual.alpha);
      glBegin(GL_QUADS);
      glVertex2f(cx - s, cy - s);
      glVertex2f(cx + s, cy - s);
      glVertex2f(cx + s, cy + s);
      glVertex2f(cx - s, cy + s);
      glEnd();
      break;
    }
  }
}
}

void reset_model_render_counters() { gModelCounters = {}; reset_runtime_animation_counters(); }

const ModelRenderCounters& model_render_counters() { return gModelCounters; }

void draw_model_instance(const ModelInstanceDesc& instance) {
  ++gModelCounters.modelResolveCount;
  ++gModelCounters.activeModelInstances;
  if (instance.lodTier == ContentLodTier::Near) ++gModelCounters.lodNearInstances;
  else if (instance.lodTier == ContentLodTier::Mid) ++gModelCounters.lodMidInstances;
  else ++gModelCounters.lodFarInstances;

  const auto resolved = gModelCache.resolve(instance.meshId, instance.lodGroup, instance.lodTier);
  if (resolved.fallback) ++gModelCounters.modelFallbackCount;

  const auto animation = resolve_runtime_animation({instance.stableId,
                                                   instance.presentationTick,
                                                   instance.animationState,
                                                   &resolved.model,
                                                   &instance.animation});

  const float scale = lod_scale(instance.lodTier);
  const float base = instance.footprint * resolved.model.footprint * scale;
  const float height = base * resolved.model.height;

  glBegin(GL_QUADS);
  const float animPulse = animation.animated ? (0.9f + 0.2f * std::sin(animation.normalizedTime * 6.2831853f)) : 1.0f;
  glColor3f(instance.tint[0] * 0.35f * animPulse, instance.tint[1] * 0.35f * animPulse, instance.tint[2] * 0.35f * animPulse);
  glVertex2f(instance.pos.x - base, instance.pos.y - base);
  glVertex2f(instance.pos.x + base, instance.pos.y - base);
  glVertex2f(instance.pos.x + base, instance.pos.y + base);
  glVertex2f(instance.pos.x - base, instance.pos.y + base);

  const float activeBoost = animation.playEvent ? 1.25f : (animation.animated ? 1.05f : 1.0f);
  const float fallbackDim = (resolved.fallback || animation.fallback) ? 0.9f : 1.0f;
  glColor3f(std::min(1.0f, instance.tint[0] * 0.68f * activeBoost * fallbackDim),
            std::min(1.0f, instance.tint[1] * 0.68f * activeBoost * fallbackDim),
            std::min(1.0f, instance.tint[2] * 0.68f * activeBoost * fallbackDim));
  glVertex2f(instance.pos.x - base * 0.85f, instance.pos.y - base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x + base * 0.85f, instance.pos.y - base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x + base * 0.85f, instance.pos.y + base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x - base * 0.85f, instance.pos.y + base * 0.85f + height * 0.2f);
  glEnd();

  std::vector<std::pair<std::string, std::string>> sortedHooks;
  sortedHooks.reserve(instance.attachmentHooks.size());
  for (const auto& kv : instance.attachmentHooks) sortedHooks.emplace_back(kv.first, kv.second);
  std::sort(sortedHooks.begin(), sortedHooks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  const auto& animCounters = runtime_animation_counters();
  gModelCounters.animationResolveCount = animCounters.animationResolveCount;
  gModelCounters.animationFallbackCount = animCounters.animationFallbackCount;
  gModelCounters.activeAnimatedInstances = animCounters.activeAnimatedInstances;
  gModelCounters.clipPlayEvents = animCounters.clipPlayEvents;
  gModelCounters.loopingClipInstances = animCounters.loopingClipInstances;

  for (const auto& [semanticName, hookName] : sortedHooks) {
    ModelAttachmentSemantic semantic{};
    if (!to_semantic(semanticName, semantic)) continue;
    ++gModelCounters.attachmentResolveCount;
    const auto hook = gModelCache.resolve_attachment_hook(resolved.resolvedAssetId, semanticName, hookName.empty() ? semanticName : hookName);
    if (hook.fallback || !hook.valid) ++gModelCounters.attachmentFallbackCount;
    draw_attachment(instance, base, height, semantic, hook, animation, hook.fallback || !hook.valid);
  }

  if (instance.selected) {
    glBegin(GL_LINE_LOOP);
    glColor3f(1.0f, 0.92f, 0.25f);
    glVertex2f(instance.pos.x - base * 1.2f, instance.pos.y - base * 1.2f);
    glVertex2f(instance.pos.x + base * 1.2f, instance.pos.y - base * 1.2f);
    glVertex2f(instance.pos.x + base * 1.2f, instance.pos.y + base * 1.2f);
    glVertex2f(instance.pos.x - base * 1.2f, instance.pos.y + base * 1.2f);
    glEnd();
  }
}

} // namespace dom::render
