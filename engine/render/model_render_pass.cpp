#include "engine/render/model_render_pass.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

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
