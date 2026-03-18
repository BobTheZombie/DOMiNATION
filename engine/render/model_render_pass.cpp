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
constexpr float kTau = 6.2831853f;

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

std::array<float, 3> mix_color(const std::array<float, 3>& a, const std::array<float, 3>& b, float t) {
  const float k = std::clamp(t, 0.0f, 1.0f);
  return {a[0] * (1.0f - k) + b[0] * k, a[1] * (1.0f - k) + b[1] * k, a[2] * (1.0f - k) + b[2] * k};
}

std::array<float, 3> mul_color(const std::array<float, 3>& a, float m) {
  return {std::clamp(a[0] * m, 0.0f, 1.0f), std::clamp(a[1] * m, 0.0f, 1.0f), std::clamp(a[2] * m, 0.0f, 1.0f)};
}

std::array<float, 3> saturate_color(const std::array<float, 3>& c, float saturation) {
  const float grey = c[0] * 0.299f + c[1] * 0.587f + c[2] * 0.114f;
  const float s = std::clamp(saturation, 0.0f, 2.0f);
  return {std::clamp(grey + (c[0] - grey) * s, 0.0f, 1.0f),
          std::clamp(grey + (c[1] - grey) * s, 0.0f, 1.0f),
          std::clamp(grey + (c[2] - grey) * s, 0.0f, 1.0f)};
}

AttachmentVisualState attachment_visual_state(ModelAttachmentSemantic semantic,
                                              const ModelInstanceDesc& instance,
                                              const RuntimeAnimationState& animation) {
  AttachmentVisualState out{};
  const float pulse = stable_phase(instance, 0.071f) * 0.5f + 0.5f;
  const float animPulse = animation.animated ? (0.45f + 0.55f * std::sin(animation.normalizedTime * kTau)) : pulse;

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
                     bool fallbackHook,
                     const std::array<float, 3>& litTint) {
  const auto visual = attachment_visual_state(semantic, instance, animation);
  if (!visual.enabled) return;

  ++gModelCounters.activeAttachmentInstances;

  const auto col = semantic_color(semantic, litTint, instance.strategicWarning || instance.guardianActive || animation.playEvent);
  const float fallbackShift = fallbackHook ? 0.04f * base : 0.0f;
  const float cx = instance.pos.x + hook.normalizedOffset[0] * base + fallbackShift;
  const float cy = instance.pos.y + height * (0.3f + hook.normalizedOffset[1]) + visual.heightBias * base;

  switch (semantic) {
    case ModelAttachmentSemantic::GuardianAura: {
      const int seg = 24;
      const float radius = base * (0.8f + visual.intensity * 0.5f) * visual.sizeScale;
      glColor4f(col[0], col[1], col[2], visual.alpha * 0.55f);
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(instance.pos.x, instance.pos.y);
      for (int i = 0; i <= seg; ++i) {
        float a = kTau * static_cast<float>(i) / static_cast<float>(seg);
        float wobble = 0.92f + 0.08f * std::sin(a * 3.0f + animation.normalizedTime * kTau);
        glVertex2f(instance.pos.x + std::cos(a) * radius * wobble, instance.pos.y + std::sin(a) * radius * wobble);
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

} // namespace

void reset_model_render_counters() {
  gModelCounters = {};
  reset_runtime_animation_counters();
}

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
  const float animPulse = animation.animated ? (0.9f + 0.2f * std::sin(animation.normalizedTime * kTau)) : 1.0f;
  const float activeBoost = animation.playEvent ? 1.25f : (animation.animated ? 1.05f : 1.0f);
  const float fallbackDim = (resolved.fallback || animation.fallback) ? 0.9f : 1.0f;
  const bool farLod = instance.lodTier == ContentLodTier::Far;

  auto litTint = instance.tint;
  if (instance.terrainAware) {
    litTint = mix_color(litTint, instance.terrainColor, instance.readability.terrainBlend * 0.5f + 0.06f);
    ++gModelCounters.terrainAwareInstances;
  }

  if (instance.readability.civTintStrength > 0.01f) ++gModelCounters.civTintInstances;
  if (instance.damaged) ++gModelCounters.damagedContrastInstances;
  if (farLod && instance.readability.farDistanceBoost > 0.01f) ++gModelCounters.farReadabilityBoostInstances;
  if (instance.strategicWarning) ++gModelCounters.warningHighlightInstances;
  if (instance.activeIndustry) ++gModelCounters.industrialHighlightInstances;
  if (instance.guardianActive || instance.guardianRevealed) ++gModelCounters.guardianHighlightInstances;

  const float ambient = std::clamp(0.34f + instance.readability.ambientBoost + instance.terrainAmbient * 0.34f, 0.25f, 1.25f);
  const float directional = std::clamp(0.30f + instance.readability.directionalBoost + instance.terrainDirectional * 0.42f, 0.2f, 1.35f);
  const float rim = std::clamp(instance.readability.rimLight + (farLod ? instance.readability.farDistanceBoost * 0.18f : 0.0f), 0.0f, 1.0f);
  const float stateContrast = std::clamp(instance.readability.stateContrast + instance.terrainContrast * 0.28f + (farLod ? instance.readability.farDistanceBoost * 0.22f : 0.0f), 0.0f, 1.0f);
  const float saturation = 1.0f + instance.readability.civTintStrength * 0.12f - (instance.damaged ? instance.readability.damageDesaturate * 0.48f : 0.0f);
  litTint = saturate_color(litTint, saturation);

  if (instance.selected) litTint = mix_color(litTint, {0.95f, 0.92f, 0.38f}, 0.16f + stateContrast * 0.16f);
  if (instance.activeIndustry) litTint = mix_color(litTint, {0.88f, 0.58f, 0.32f}, instance.readability.industrialHighlight * 0.18f);
  if (instance.strategicWarning) litTint = mix_color(litTint, {0.96f, 0.34f, 0.22f}, 0.18f + instance.readability.warningHighlight * 0.22f);
  if (instance.guardianActive || instance.guardianRevealed) {
    const auto guardianCol = instance.guardianActive ? std::array<float, 3>{0.82f, 0.36f, 0.96f} : std::array<float, 3>{0.68f, 0.42f, 0.92f};
    litTint = mix_color(litTint, guardianCol, 0.16f + instance.readability.guardianHighlight * 0.18f);
  }

  const auto groundBlend = instance.terrainAware ? mix_color(instance.terrainColor, instance.terrainAccent, 0.45f + instance.terrainContrast * 0.28f) : instance.terrainColor;
  const auto shadowColor = mul_color(mix_color(groundBlend, litTint, 0.20f), 0.45f + ambient * 0.10f);
  const auto bodyColor = mul_color(litTint, (0.52f + ambient * 0.18f) * animPulse * fallbackDim);
  const auto topColor = mul_color(mix_color(litTint, instance.terrainAccent, instance.terrainAware ? instance.readability.terrainBlend * 0.22f : 0.0f),
                                  std::min(1.35f, 0.72f + directional * 0.34f + activeBoost * 0.12f + rim * 0.18f));
  const auto rimColor = mix_color(topColor, {1.0f, 1.0f, 1.0f}, 0.18f + rim * 0.22f + stateContrast * 0.10f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const float shadowOffset = base * (0.10f + directional * 0.05f);
  glColor4f(shadowColor[0], shadowColor[1], shadowColor[2], 0.28f + stateContrast * 0.08f);
  glBegin(GL_QUADS);
  glVertex2f(instance.pos.x - base * 1.02f + shadowOffset, instance.pos.y - base * 0.92f - shadowOffset * 0.2f);
  glVertex2f(instance.pos.x + base * 1.02f + shadowOffset, instance.pos.y - base * 0.92f - shadowOffset * 0.2f);
  glVertex2f(instance.pos.x + base * 0.94f + shadowOffset, instance.pos.y + base * 0.90f - shadowOffset * 0.2f);
  glVertex2f(instance.pos.x - base * 0.94f + shadowOffset, instance.pos.y + base * 0.90f - shadowOffset * 0.2f);
  glEnd();

  glBegin(GL_QUADS);
  glColor4f(shadowColor[0], shadowColor[1], shadowColor[2], 0.96f);
  glVertex2f(instance.pos.x - base, instance.pos.y - base);
  glVertex2f(instance.pos.x + base, instance.pos.y - base);
  glColor4f(bodyColor[0], bodyColor[1], bodyColor[2], 1.0f);
  glVertex2f(instance.pos.x + base * 0.96f, instance.pos.y + base * 0.96f);
  glVertex2f(instance.pos.x - base * 0.96f, instance.pos.y + base * 0.96f);

  glColor4f(bodyColor[0], bodyColor[1], bodyColor[2], 1.0f);
  glVertex2f(instance.pos.x - base * 0.86f, instance.pos.y - base * 0.82f + height * 0.16f);
  glVertex2f(instance.pos.x + base * 0.86f, instance.pos.y - base * 0.82f + height * 0.16f);
  glColor4f(topColor[0], topColor[1], topColor[2], 1.0f);
  glVertex2f(instance.pos.x + base * 0.84f, instance.pos.y + base * 0.82f + height * 0.18f);
  glVertex2f(instance.pos.x - base * 0.84f, instance.pos.y + base * 0.82f + height * 0.18f);
  glEnd();

  glLineWidth(farLod ? 1.5f : 1.0f);
  glColor4f(rimColor[0], rimColor[1], rimColor[2], 0.65f + stateContrast * 0.18f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(instance.pos.x - base * 0.88f, instance.pos.y - base * 0.84f + height * 0.14f);
  glVertex2f(instance.pos.x + base * 0.88f, instance.pos.y - base * 0.84f + height * 0.14f);
  glVertex2f(instance.pos.x + base * 0.86f, instance.pos.y + base * 0.84f + height * 0.18f);
  glVertex2f(instance.pos.x - base * 0.86f, instance.pos.y + base * 0.84f + height * 0.18f);
  glEnd();
  glLineWidth(1.0f);

  float emissive = instance.readability.emissiveStrength;
  if (instance.activeIndustry) emissive += instance.readability.industrialHighlight * 0.55f;
  if (instance.strategicWarning) emissive += instance.readability.warningHighlight * 0.70f;
  if (instance.guardianActive || instance.guardianRevealed) emissive += instance.readability.guardianHighlight * (instance.guardianActive ? 0.72f : 0.38f);
  if (instance.combatFiring) emissive += 0.24f;
  emissive += farLod ? instance.readability.farDistanceBoost * 0.24f : 0.0f;
  emissive = std::clamp(emissive, 0.0f, 1.0f);
  if (emissive > 0.04f) {
    ++gModelCounters.emissiveAccentInstances;
    auto emissiveColor = litTint;
    if (instance.strategicWarning) emissiveColor = {1.0f, 0.36f, 0.24f};
    else if (instance.guardianActive || instance.guardianRevealed) emissiveColor = instance.guardianActive ? std::array<float, 3>{0.84f, 0.42f, 0.98f} : std::array<float, 3>{0.70f, 0.48f, 0.94f};
    else if (instance.activeIndustry) emissiveColor = {0.96f, 0.72f, 0.34f};
    else if (instance.combatFiring) emissiveColor = {1.0f, 0.88f, 0.30f};
    emissiveColor = mix_color(emissiveColor, {1.0f, 1.0f, 1.0f}, 0.25f + emissive * 0.18f);
    const float band = base * (farLod ? 0.16f : 0.12f);
    const float pulse = 0.75f + 0.25f * (stable_phase(instance, 0.05f) * 0.5f + 0.5f);
    glColor4f(emissiveColor[0], emissiveColor[1], emissiveColor[2], (0.16f + emissive * 0.28f) * pulse);
    glBegin(GL_QUADS);
    glVertex2f(instance.pos.x - band, instance.pos.y - base * 0.18f + height * 0.24f);
    glVertex2f(instance.pos.x + band, instance.pos.y - base * 0.18f + height * 0.24f);
    glVertex2f(instance.pos.x + band * 0.8f, instance.pos.y + base * 0.58f + height * 0.24f);
    glVertex2f(instance.pos.x - band * 0.8f, instance.pos.y + base * 0.58f + height * 0.24f);
    glEnd();
  }

  if (instance.damaged) {
    const float stripe = base * 0.12f;
    glColor4f(0.18f, 0.08f, 0.08f, 0.22f + stateContrast * 0.16f);
    glBegin(GL_QUADS);
    glVertex2f(instance.pos.x - base * 0.62f, instance.pos.y - stripe + height * 0.12f);
    glVertex2f(instance.pos.x + base * 0.62f, instance.pos.y - stripe * 0.15f + height * 0.12f);
    glVertex2f(instance.pos.x + base * 0.54f, instance.pos.y + stripe * 0.55f + height * 0.12f);
    glVertex2f(instance.pos.x - base * 0.70f, instance.pos.y - stripe * 0.30f + height * 0.12f);
    glEnd();
  }

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
    draw_attachment(instance, base, height, semantic, hook, animation, hook.fallback || !hook.valid, litTint);
  }

  if (instance.selected) {
    glColor4f(1.0f, 0.92f, 0.25f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(instance.pos.x - base * 1.18f, instance.pos.y - base * 1.18f);
    glVertex2f(instance.pos.x + base * 1.18f, instance.pos.y - base * 1.18f);
    glVertex2f(instance.pos.x + base * 1.18f, instance.pos.y + base * 1.18f);
    glVertex2f(instance.pos.x - base * 1.18f, instance.pos.y + base * 1.18f);
    glEnd();
  }

  glDisable(GL_BLEND);
}

} // namespace dom::render
