#include "engine/render/renderer.h"
#include <GL/gl.h>
#include <cmath>
#include <glm/geometric.hpp>
#include <algorithm>
#include <array>
#include <vector>

namespace dom::render {
namespace {
struct OverlayState {
  bool showTerritory{true};
  bool showBorders{true};
  bool showFog{true};
  GLuint territoryTex{0};
  GLuint borderTex{0};
  GLuint fogTex{0};
  int texW{0};
  int texH{0};
  std::vector<uint8_t> territory;
  std::vector<uint8_t> border;
  std::vector<uint8_t> fog;
};

OverlayState gOverlay;

std::array<std::array<float, 3>, 4> kTeamColors{{
    {0.0f, 0.0f, 0.0f},
    {0.90f, 0.25f, 0.25f},
    {0.25f, 0.45f, 0.95f},
    {0.20f, 0.85f, 0.35f},
}};

void ensure_overlay_textures(const dom::sim::World& w) {
  if (gOverlay.texW == w.width && gOverlay.texH == w.height && gOverlay.territoryTex != 0) return;
  gOverlay.texW = w.width;
  gOverlay.texH = w.height;
  gOverlay.territory.assign(w.width * w.height, 0);
  gOverlay.border.assign(w.width * w.height, 0);
  gOverlay.fog.assign(w.width * w.height, 0);

  if (gOverlay.territoryTex == 0) glGenTextures(1, &gOverlay.territoryTex);
  if (gOverlay.borderTex == 0) glGenTextures(1, &gOverlay.borderTex);
  if (gOverlay.fogTex == 0) glGenTextures(1, &gOverlay.fogTex);

  auto initTex = [&](GLuint tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w.width, w.height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
  };

  initTex(gOverlay.territoryTex);
  initTex(gOverlay.borderTex);
  initTex(gOverlay.fogTex);
}

void upload_overlay(GLuint tex, const std::vector<uint8_t>& src, int w, int h) {
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, src.data());
}

void update_overlay_textures(dom::sim::World& w) {
  ensure_overlay_textures(w);

  if (w.territoryDirty) {
    for (size_t i = 0; i < w.territoryOwner.size(); ++i) {
      gOverlay.territory[i] = static_cast<uint8_t>(w.territoryOwner[i] & 0xFFu);
    }

    for (int y = 0; y < w.height; ++y) {
      for (int x = 0; x < w.width; ++x) {
        int i = y * w.width + x;
        uint16_t owner = w.territoryOwner[i];
        bool edge = false;
        if (x > 0 && w.territoryOwner[i - 1] != owner) edge = true;
        if (x + 1 < w.width && w.territoryOwner[i + 1] != owner) edge = true;
        if (y > 0 && w.territoryOwner[i - w.width] != owner) edge = true;
        if (y + 1 < w.height && w.territoryOwner[i + w.width] != owner) edge = true;
        gOverlay.border[i] = edge ? 255 : 0;
      }
    }

    upload_overlay(gOverlay.territoryTex, gOverlay.territory, w.width, w.height);
    upload_overlay(gOverlay.borderTex, gOverlay.border, w.width, w.height);
    w.territoryDirty = false;
  }

  if (w.fogDirty) {
    gOverlay.fog = w.fog;
    upload_overlay(gOverlay.fogTex, gOverlay.fog, w.width, w.height);
    w.fogDirty = false;
  }
}

void draw_textured_overlay(GLuint tex, const dom::sim::World& w, float alpha, const std::array<float, 3>& color, bool invert = false) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(color[0], color[1], color[2], alpha);
  glBegin(GL_QUADS);
  float ax = static_cast<float>(w.width);
  float ay = static_cast<float>(w.height);
  if (!invert) {
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(ax, 0);
    glTexCoord2f(1, 1); glVertex2f(ax, ay);
    glTexCoord2f(0, 1); glVertex2f(0, ay);
  } else {
    glTexCoord2f(0, 1); glVertex2f(0, 0);
    glTexCoord2f(1, 1); glVertex2f(ax, 0);
    glTexCoord2f(1, 0); glVertex2f(ax, ay);
    glTexCoord2f(0, 0); glVertex2f(0, ay);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}

struct ClusterBin {
  glm::vec2 center{};
  int count{0};
  uint16_t team{0};
};

} // namespace

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
  float best = camera.zoom > 70.0f ? 4.5f : 2.0f;
  for (const auto& u : world.units) {
    float d = glm::length(u.pos - p);
    if (d < best) { best = d; id = u.id; }
  }
  return id;
}

void draw(dom::sim::World& w, const Camera& c, int width, int height) {
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
      float r = 0.15f + 0.2f * f;
      float g = 0.35f + 0.4f * f;
      float b = 0.15f + 0.1f * (h + 1.0f);
      glColor3f(r, g, b);
      glVertex2f(x, y); glVertex2f(x + 1, y); glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
    }
  }
  glEnd();

  update_overlay_textures(w);

  if (gOverlay.showTerritory) {
    draw_textured_overlay(gOverlay.territoryTex, w, 0.20f, {0.55f, 0.55f, 0.95f}, true);
  }
  if (gOverlay.showBorders) {
    draw_textured_overlay(gOverlay.borderTex, w, 0.45f, {0.98f, 0.95f, 0.5f}, true);
  }
  if (gOverlay.showFog && !w.godMode) {
    draw_textured_overlay(gOverlay.fogTex, w, 0.55f, {0.0f, 0.0f, 0.0f}, true);
  }

  const float nearThreshold = 22.0f;
  const float farThreshold = 58.0f;
  const float clusterThreshold = 95.0f;

  if (c.zoom >= clusterThreshold) {
    const float cell = std::max(6.0f, c.zoom * 0.18f);
    int cols = std::max(1, static_cast<int>(std::ceil(w.width / cell)));
    int rows = std::max(1, static_cast<int>(std::ceil(w.height / cell)));
    std::vector<ClusterBin> bins(cols * rows);
    for (const auto& u : w.units) {
      int gx = std::clamp(static_cast<int>(u.pos.x / cell), 0, cols - 1);
      int gy = std::clamp(static_cast<int>(u.pos.y / cell), 0, rows - 1);
      auto& b = bins[gy * cols + gx];
      b.center = {(gx + 0.5f) * cell, (gy + 0.5f) * cell};
      b.team = u.team;
      ++b.count;
    }

    glBegin(GL_QUADS);
    for (const auto& b : bins) {
      if (b.count == 0) continue;
      auto tc = kTeamColors[std::min<size_t>(b.team + 1, 3)];
      glColor3f(tc[0], tc[1], tc[2]);
      float s = 0.9f + std::min(2.0f, b.count / 8.0f);
      glVertex2f(b.center.x - s, b.center.y - s);
      glVertex2f(b.center.x + s, b.center.y - s);
      glVertex2f(b.center.x + s, b.center.y + s);
      glVertex2f(b.center.x - s, b.center.y + s);
    }
    glEnd();
  } else {
    for (const auto& u : w.units) {
      if (!w.godMode) {
        int x = std::clamp((int)u.pos.x, 0, w.width - 1);
        int y = std::clamp((int)u.pos.y, 0, w.height - 1);
        if (w.fog[y * w.width + x] == 0 && u.team != 0) continue;
      }

      auto tc = kTeamColors[std::min<size_t>(u.team + 1, 3)];
      if (c.zoom < nearThreshold) {
        glBegin(GL_TRIANGLES);
        glColor3f(tc[0], tc[1], tc[2]);
        float s = 0.35f;
        glVertex2f(u.renderPos.x, u.renderPos.y + s);
        glVertex2f(u.renderPos.x - s, u.renderPos.y - s);
        glVertex2f(u.renderPos.x + s, u.renderPos.y - s);
        glEnd();
      } else if (c.zoom < farThreshold) {
        glBegin(GL_QUADS);
        glColor3f(tc[0], tc[1], tc[2]);
        float s = 0.42f;
        glVertex2f(u.renderPos.x - s, u.renderPos.y - s);
        glVertex2f(u.renderPos.x + s, u.renderPos.y - s);
        glVertex2f(u.renderPos.x + s, u.renderPos.y + s);
        glVertex2f(u.renderPos.x - s, u.renderPos.y + s);
        glEnd();
      } else {
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(tc[0], tc[1], tc[2]);
        float s = 0.44f;
        glVertex2f(u.renderPos.x, u.renderPos.y);
        for (int i = 0; i <= 10; ++i) {
          float a = i * 0.62831853f;
          glVertex2f(u.renderPos.x + std::cos(a) * s, u.renderPos.y + std::sin(a) * s);
        }
        glEnd();
      }
    }
  }

  if (w.godMode) {
    glBegin(GL_QUADS);
    for (const auto& cty : w.cities) {
      auto tc = kTeamColors[std::min<size_t>(cty.team + 1, 3)];
      glColor3f(tc[0], tc[1], tc[2]);
      float s = cty.capital ? 1.2f : 0.8f;
      glVertex2f(cty.pos.x - s, cty.pos.y - s);
      glVertex2f(cty.pos.x + s, cty.pos.y - s);
      glVertex2f(cty.pos.x + s, cty.pos.y + s);
      glVertex2f(cty.pos.x - s, cty.pos.y + s);
    }
    glEnd();
  }
}

void toggle_territory_overlay() { gOverlay.showTerritory = !gOverlay.showTerritory; }
void toggle_border_overlay() { gOverlay.showBorders = !gOverlay.showBorders; }
void toggle_fog_overlay() { gOverlay.showFog = !gOverlay.showFog; }

} // namespace dom::render
