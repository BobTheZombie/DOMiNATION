#include "engine/render/model_render_pass.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dom::render {
namespace {
ModelCache gModelCache{};
ModelRenderCounters gModelCounters{};

float lod_scale(ContentLodTier tier) {
  switch (tier) {
    case ContentLodTier::Near: return 1.0f;
    case ContentLodTier::Mid: return 0.8f;
    case ContentLodTier::Far: return 0.62f;
  }
  return 1.0f;
}

bool semantic_enabled(ModelAttachmentSemantic semantic, const ModelInstanceDesc& instance) {
  switch (semantic) {
    case ModelAttachmentSemantic::BannerSocket: return true;
    case ModelAttachmentSemantic::CivEmblem: return true;
    case ModelAttachmentSemantic::SmokeStack: return instance.activeIndustry;
    case ModelAttachmentSemantic::MuzzleFlash: return instance.combatFiring;
    case ModelAttachmentSemantic::SelectionBadge: return instance.selected;
    case ModelAttachmentSemantic::WarningBadge: return instance.strategicWarning || instance.damaged;
    case ModelAttachmentSemantic::GuardianAura: return instance.guardianActive || instance.guardianRevealed;
  }
  return false;
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
                     bool fallbackHook) {
  if (!semantic_enabled(semantic, instance)) return;

  ++gModelCounters.activeAttachmentInstances;

  const float phase = std::sin(static_cast<float>(instance.pos.x * 0.43 + instance.pos.y * 0.29));
  const float pulse = 0.03f + (phase * 0.5f + 0.5f) * 0.05f;
  const auto col = semantic_color(semantic, instance.tint, instance.guardianActive || instance.combatFiring || instance.strategicWarning);

  float ox = hook.normalizedOffset.x;
  float oy = hook.normalizedOffset.y;
  if (fallbackHook || !hook.valid) {
    ox = 0.0f;
    oy = 0.0f;
  }
  float cx = instance.pos.x + ox * base;
  float cy = instance.pos.y + oy * base + hook.normalizedOffset.y * height * 0.22f;

  switch (semantic) {
    case ModelAttachmentSemantic::SmokeStack: {
      glColor4f(col[0], col[1], col[2], 0.5f);
      glBegin(GL_TRIANGLES);
      glVertex2f(cx, cy + base * (0.36f + pulse));
      glVertex2f(cx - base * 0.17f, cy - base * 0.08f);
      glVertex2f(cx + base * 0.17f, cy - base * 0.08f);
      glEnd();
      break;
    }
    case ModelAttachmentSemantic::GuardianAura: {
      const float radius = base * (1.15f + pulse * 2.0f);
      glBegin(GL_LINE_LOOP);
      glColor3f(col[0], col[1], col[2]);
      for (int i = 0; i < 20; ++i) {
        float a = static_cast<float>(i) * 0.31415926f;
        glVertex2f(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
      }
      glEnd();
      break;
    }
    default: {
      float s = base * (semantic == ModelAttachmentSemantic::MuzzleFlash ? 0.22f + pulse : 0.16f + pulse * 0.7f);
      glColor3f(col[0], col[1], col[2]);
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

void reset_model_render_counters() { gModelCounters = {}; }

const ModelRenderCounters& model_render_counters() { return gModelCounters; }

void draw_model_instance(const ModelInstanceDesc& instance) {
  ++gModelCounters.modelResolveCount;
  ++gModelCounters.activeModelInstances;
  if (instance.lodTier == ContentLodTier::Near) ++gModelCounters.lodNearInstances;
  else if (instance.lodTier == ContentLodTier::Mid) ++gModelCounters.lodMidInstances;
  else ++gModelCounters.lodFarInstances;

  const auto resolved = gModelCache.resolve(instance.meshId, instance.lodGroup, instance.lodTier);
  if (resolved.fallback) ++gModelCounters.modelFallbackCount;

  const float scale = lod_scale(instance.lodTier);
  const float base = instance.footprint * resolved.model.footprint * scale;
  const float height = base * resolved.model.height;

  glBegin(GL_QUADS);
  glColor3f(instance.tint[0] * 0.35f, instance.tint[1] * 0.35f, instance.tint[2] * 0.35f);
  glVertex2f(instance.pos.x - base, instance.pos.y - base);
  glVertex2f(instance.pos.x + base, instance.pos.y - base);
  glVertex2f(instance.pos.x + base, instance.pos.y + base);
  glVertex2f(instance.pos.x - base, instance.pos.y + base);

  glColor3f(instance.tint[0] * 0.68f, instance.tint[1] * 0.68f, instance.tint[2] * 0.68f);
  glVertex2f(instance.pos.x - base * 0.85f, instance.pos.y - base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x + base * 0.85f, instance.pos.y - base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x + base * 0.85f, instance.pos.y + base * 0.85f + height * 0.2f);
  glVertex2f(instance.pos.x - base * 0.85f, instance.pos.y + base * 0.85f + height * 0.2f);
  glEnd();

  std::vector<std::pair<std::string, std::string>> sortedHooks;
  sortedHooks.reserve(instance.attachmentHooks.size());
  for (const auto& kv : instance.attachmentHooks) sortedHooks.emplace_back(kv.first, kv.second);
  std::sort(sortedHooks.begin(), sortedHooks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  for (const auto& [semanticName, hookName] : sortedHooks) {
    ModelAttachmentSemantic semantic{};
    if (!to_semantic(semanticName, semantic)) continue;
    ++gModelCounters.attachmentResolveCount;
    const auto hook = gModelCache.resolve_attachment_hook(hookName.empty() ? semanticName : hookName);
    if (hook.fallback || !hook.valid) ++gModelCounters.attachmentFallbackCount;
    draw_attachment(instance, base, height, semantic, hook, hook.fallback || !hook.valid);
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
