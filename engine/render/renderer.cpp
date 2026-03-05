#include "engine/render/renderer.h"
#include <GL/gl.h>
#include <cmath>
#include <glm/geometric.hpp>
#include <algorithm>

namespace dom::render {

bool init_renderer() {
  glClearColor(0.08f, 0.1f, 0.14f, 1.0f);
  return true;
}

glm::vec2 screen_to_world(const Camera& camera, int width, int height, glm::vec2 s) {
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  float wx = ((s.x / width) * 2.0f - 1.0f) * camera.zoom * aspect + camera.center.x;
  float wy = ((1.0f - s.y / height) * 2.0f - 1.0f) * camera.zoom + camera.center.y;
  return {wx, wy};
}

uint32_t pick_unit(const dom::sim::World& world, const Camera& camera, int width, int height, glm::vec2 s) {
  glm::vec2 p = screen_to_world(camera, width, height, s);
  uint32_t id = 0;
  float best = 2.0f;
  for (const auto& u : world.units) {
    float d = glm::length(u.pos - p);
    if (d < best) { best = d; id = u.id; }
  }
  return id;
}

void draw(const dom::sim::World& w, const Camera& c, int width, int height) {
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  glOrtho(c.center.x - c.zoom * aspect, c.center.x + c.zoom * aspect, c.center.y - c.zoom, c.center.y + c.zoom, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glBegin(GL_QUADS);
  for (int y = 0; y < w.height - 1; ++y) {
    for (int x = 0; x < w.width - 1; ++x) {
      size_t i = y * w.width + x;
      float h = w.heightmap[i];
      float f = w.fertility[i];
      uint16_t owner = w.territoryOwner[i];
      float r = 0.15f + 0.2f * f;
      float g = 0.35f + 0.4f * f;
      float b = 0.15f + 0.1f * (h + 1.0f);
      if (owner == 0) r += 0.12f;
      if (owner == 1) b += 0.2f;
      if (!w.godMode && w.fog[i] == 0) { r *= 0.45f; g *= 0.45f; b *= 0.45f; }
      glColor3f(r, g, b);
      glVertex2f(x, y); glVertex2f(x + 1, y); glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
    }
  }
  glEnd();

  glBegin(GL_TRIANGLES);
  for (const auto& u : w.units) {
    if (!w.godMode) {
      int x = std::clamp((int)u.pos.x, 0, w.width - 1);
      int y = std::clamp((int)u.pos.y, 0, w.height - 1);
      if (w.fog[y * w.width + x] == 0 && u.team != 0) continue;
    }
    glColor3f(u.team == 0 ? 0.9f : 0.9f, u.team == 0 ? 0.9f : 0.2f, u.team == 0 ? 0.2f : 0.2f);
    float s = c.zoom > 25.0f ? 0.65f : 0.35f;
    glVertex2f(u.renderPos.x, u.renderPos.y + s);
    glVertex2f(u.renderPos.x - s, u.renderPos.y - s);
    glVertex2f(u.renderPos.x + s, u.renderPos.y - s);
  }
  glEnd();
}

} // namespace dom::render
