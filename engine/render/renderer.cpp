#include "engine/render/renderer.h"
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <unordered_set>
#include <vector>

namespace dom::render {
namespace {
struct OverlayState {
  bool showTerritory{true};
  bool showBorders{true};
  bool showFog{true};
  bool showMinimap{true};
  GLuint territoryTex{0};
  GLuint borderTex{0};
  GLuint fogTex{0};
  GLuint minimapTex{0};
  int texW{0};
  int texH{0};
  int minimapRes{256};
  int minimapFrameCounter{0};
  std::vector<uint8_t> territory;
  std::vector<uint8_t> border;
  std::vector<uint8_t> fog;
  std::vector<uint8_t> minimap;
};

OverlayState gOverlay;

std::array<std::array<float, 3>, 4> kTeamColors{{
    {0.0f, 0.0f, 0.0f},
    {0.90f, 0.25f, 0.25f},
    {0.25f, 0.45f, 0.95f},
    {0.20f, 0.85f, 0.35f},
}};

std::array<uint8_t, 3> team_rgb(uint16_t team) {
  auto c = kTeamColors[std::min<size_t>(team + 1, 3)];
  return {static_cast<uint8_t>(c[0] * 255.0f), static_cast<uint8_t>(c[1] * 255.0f), static_cast<uint8_t>(c[2] * 255.0f)};
}

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

void ensure_minimap_texture() {
  if (gOverlay.minimapTex == 0) glGenTextures(1, &gOverlay.minimapTex);
  if (gOverlay.minimap.empty()) {
    gOverlay.minimap.assign(gOverlay.minimapRes * gOverlay.minimapRes * 3, 0);
    glBindTexture(GL_TEXTURE_2D, gOverlay.minimapTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gOverlay.minimapRes, gOverlay.minimapRes, 0, GL_RGB, GL_UNSIGNED_BYTE, gOverlay.minimap.data());
  }
}

void upload_overlay(GLuint tex, const std::vector<uint8_t>& src, int w, int h) {
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, src.data());
}

void update_overlay_textures(dom::sim::World& w) {
  ensure_overlay_textures(w);

  if (w.territoryDirty) {
    for (size_t i = 0; i < w.territoryOwner.size(); ++i) gOverlay.territory[i] = static_cast<uint8_t>(w.territoryOwner[i] & 0xFFu);

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

void plot_dot(std::vector<uint8_t>& pix, int res, int x, int y, const std::array<uint8_t, 3>& rgb, int size) {
  for (int oy = -size; oy <= size; ++oy) {
    for (int ox = -size; ox <= size; ++ox) {
      int px = x + ox;
      int py = y + oy;
      if (px < 0 || py < 0 || px >= res || py >= res) continue;
      size_t i = static_cast<size_t>(py * res + px) * 3;
      pix[i + 0] = rgb[0];
      pix[i + 1] = rgb[1];
      pix[i + 2] = rgb[2];
    }
  }
}

int world_to_minimap_px(float coord, float worldExtent, int res) {
  float n = worldExtent > 1.0f ? coord / worldExtent : 0.0f;
  n = std::clamp(n, 0.0f, 0.999f);
  return static_cast<int>(n * res);
}

void build_minimap_pixels(const dom::sim::World& w, int res, std::vector<uint8_t>& out) {
  out.assign(res * res * 3, 0);
  for (int y = 0; y < res; ++y) {
    for (int x = 0; x < res; ++x) {
      int gx = std::clamp(static_cast<int>((static_cast<float>(x) / res) * w.width), 0, w.width - 1);
      int gy = std::clamp(static_cast<int>((static_cast<float>(y) / res) * w.height), 0, w.height - 1);
      size_t gi = static_cast<size_t>(gy * w.width + gx);
      float h = w.heightmap[gi];
      float f = w.fertility[gi];
      float r = 0.14f + 0.18f * f;
      float g = 0.28f + 0.45f * f;
      float b = 0.16f + 0.08f * (h + 1.0f);

      uint16_t owner = w.territoryOwner[gi];
      if (owner > 0) {
        auto team = team_rgb(static_cast<uint16_t>(owner - 1));
        r = std::clamp(r * 0.7f + (team[0] / 255.0f) * 0.3f, 0.0f, 1.0f);
        g = std::clamp(g * 0.7f + (team[1] / 255.0f) * 0.3f, 0.0f, 1.0f);
        b = std::clamp(b * 0.7f + (team[2] / 255.0f) * 0.3f, 0.0f, 1.0f);
      }

      if (!w.godMode && w.fog[gi] > 0) {
        r *= 0.2f;
        g *= 0.2f;
        b *= 0.24f;
      }

      size_t i = static_cast<size_t>(y * res + x) * 3;
      out[i + 0] = static_cast<uint8_t>(r * 255.0f);
      out[i + 1] = static_cast<uint8_t>(g * 255.0f);
      out[i + 2] = static_cast<uint8_t>(b * 255.0f);
    }
  }

  for (const auto& c : w.cities) {
    int gx = std::clamp(static_cast<int>(c.pos.x), 0, w.width - 1);
    int gy = std::clamp(static_cast<int>(c.pos.y), 0, w.height - 1);
    if (!w.godMode && w.fog[gy * w.width + gx] > 0 && c.team != 0) continue;
    int px = world_to_minimap_px(c.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(c.pos.y, static_cast<float>(w.height), res);
    auto rgb = team_rgb(c.team);
    plot_dot(out, res, px, py, rgb, c.capital ? 2 : 1);
  }

  for (const auto& u : w.units) {
    int gx = std::clamp(static_cast<int>(u.pos.x), 0, w.width - 1);
    int gy = std::clamp(static_cast<int>(u.pos.y), 0, w.height - 1);
    if (!w.godMode && w.fog[gy * w.width + gx] > 0 && u.team != 0) continue;
    int px = world_to_minimap_px(u.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(u.pos.y, static_cast<float>(w.height), res);
    plot_dot(out, res, px, py, team_rgb(u.team), 0);
  }
}

void draw_ring(glm::vec2 pos, float radius, float thickness, const std::array<float, 3>& color) {
  glColor3f(color[0], color[1], color[2]);
  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i <= 40; ++i) {
    float a = static_cast<float>(i) * 0.15707963f;
    float ca = std::cos(a);
    float sa = std::sin(a);
    glVertex2f(pos.x + ca * (radius - thickness), pos.y + sa * (radius - thickness));
    glVertex2f(pos.x + ca * radius, pos.y + sa * radius);
  }
  glEnd();
}

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
  float best = camera.zoom > 60.0f ? 6.0f : (camera.zoom > 30.0f ? 3.2f : 2.0f);
  for (const auto& u : world.units) {
    float d = glm::length(u.pos - p);
    if (d < best) { best = d; id = u.id; }
  }
  return id;
}

bool minimap_screen_to_world(const dom::sim::World& world, int width, int height, glm::vec2 screen, glm::vec2& outWorld) {
  const int size = 210;
  const int pad = 18;
  int x0 = width - size - pad;
  int y0 = height - size - pad;
  if (!gOverlay.showMinimap) return false;
  if (screen.x < x0 || screen.y < y0 || screen.x > x0 + size || screen.y > y0 + size) return false;
  float nx = (screen.x - x0) / static_cast<float>(size);
  float ny = (screen.y - y0) / static_cast<float>(size);
  outWorld = {nx * world.width, (1.0f - ny) * world.height};
  return true;
}

void draw(dom::sim::World& w, const Camera& c, int width, int height, const std::vector<uint32_t>& dragHighlight) {
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
      glColor3f(0.15f + 0.2f * f, 0.35f + 0.4f * f, 0.15f + 0.1f * (h + 1.0f));
      glVertex2f(x, y); glVertex2f(x + 1, y); glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
    }
  }
  glEnd();

  update_overlay_textures(w);
  if (gOverlay.showTerritory) draw_textured_overlay(gOverlay.territoryTex, w, 0.20f, {0.55f, 0.55f, 0.95f}, true);
  if (gOverlay.showBorders) draw_textured_overlay(gOverlay.borderTex, w, 0.45f, {0.98f, 0.95f, 0.5f}, true);
  if (gOverlay.showFog && !w.godMode) draw_textured_overlay(gOverlay.fogTex, w, 0.55f, {0.0f, 0.0f, 0.0f}, true);

  glBegin(GL_QUADS);
  for (const auto& b : w.buildings) {
    float r = 0.6f, g = 0.6f, bl = 0.65f;
    if (b.team == 0) { r = 0.82f; g = 0.32f; bl = 0.32f; }
    if (b.team == 1) { r = 0.28f; g = 0.45f; bl = 0.88f; }
    if (b.underConstruction) { r *= 0.6f; g *= 0.6f; bl *= 0.6f; }
    glColor3f(r, g, bl);
    float sx = b.size.x * 0.5f;
    float sy = b.size.y * 0.5f;
    glVertex2f(b.pos.x - sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y + sy);
    glVertex2f(b.pos.x - sx, b.pos.y + sy);
  }
  glEnd();

  if (w.placementActive) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(w.placementValid ? 0.2f : 0.9f, w.placementValid ? 0.9f : 0.2f, 0.2f, 0.35f);
    float sx = 1.4f;
    glBegin(GL_QUADS);
    glVertex2f(w.placementPos.x - sx, w.placementPos.y - sx);
    glVertex2f(w.placementPos.x + sx, w.placementPos.y - sx);
    glVertex2f(w.placementPos.x + sx, w.placementPos.y + sx);
    glVertex2f(w.placementPos.x - sx, w.placementPos.y + sx);
    glEnd();
    glDisable(GL_BLEND);
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
        int x = std::clamp(static_cast<int>(u.pos.x), 0, w.width - 1);
        int y = std::clamp(static_cast<int>(u.pos.y), 0, w.height - 1);
        if (w.fog[y * w.width + x] > 0 && u.team != 0) continue;
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

  std::unordered_set<uint32_t> dragSet(dragHighlight.begin(), dragHighlight.end());
  for (const auto& u : w.units) {
    if (u.selected) draw_ring(u.renderPos, 0.7f, 0.14f, {1.0f, 0.95f, 0.25f});
    else if (dragSet.contains(u.id)) draw_ring(u.renderPos, 0.62f, 0.11f, {0.85f, 0.85f, 0.85f});
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

  if (gOverlay.showMinimap) {
    ensure_minimap_texture();
    if ((gOverlay.minimapFrameCounter++ % 8) == 0) {
      build_minimap_pixels(w, gOverlay.minimapRes, gOverlay.minimap);
      glBindTexture(GL_TEXTURE_2D, gOverlay.minimapTex);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gOverlay.minimapRes, gOverlay.minimapRes, GL_RGB, GL_UNSIGNED_BYTE, gOverlay.minimap.data());
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float size = 210.0f;
    const float pad = 18.0f;
    const float x0 = width - size - pad;
    const float y0 = height - size - pad;

    glColor3f(0.06f, 0.06f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(x0 - 3, y0 - 3); glVertex2f(x0 + size + 3, y0 - 3);
    glVertex2f(x0 + size + 3, y0 + size + 3); glVertex2f(x0 - 3, y0 + size + 3);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gOverlay.minimapTex);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(x0, y0);
    glTexCoord2f(1, 1); glVertex2f(x0 + size, y0);
    glTexCoord2f(1, 0); glVertex2f(x0 + size, y0 + size);
    glTexCoord2f(0, 0); glVertex2f(x0, y0 + size);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    float worldX0 = (c.center.x - c.zoom * aspect) / static_cast<float>(w.width);
    float worldX1 = (c.center.x + c.zoom * aspect) / static_cast<float>(w.width);
    float worldY0 = (c.center.y - c.zoom) / static_cast<float>(w.height);
    float worldY1 = (c.center.y + c.zoom) / static_cast<float>(w.height);
    worldX0 = std::clamp(worldX0, 0.0f, 1.0f);
    worldX1 = std::clamp(worldX1, 0.0f, 1.0f);
    worldY0 = std::clamp(worldY0, 0.0f, 1.0f);
    worldY1 = std::clamp(worldY1, 0.0f, 1.0f);

    float vx0 = x0 + worldX0 * size;
    float vx1 = x0 + worldX1 * size;
    float vy0 = y0 + (1.0f - worldY1) * size;
    float vy1 = y0 + (1.0f - worldY0) * size;
    glColor3f(1.0f, 1.0f, 0.8f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(vx0, vy0); glVertex2f(vx1, vy0); glVertex2f(vx1, vy1); glVertex2f(vx0, vy1);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  }
}

void generate_minimap_image(const dom::sim::World& world, int resolution, std::vector<uint8_t>& outRgb) {
  int res = std::max(32, resolution);
  build_minimap_pixels(world, res, outRgb);
}

void toggle_minimap() { gOverlay.showMinimap = !gOverlay.showMinimap; }
void toggle_territory_overlay() { gOverlay.showTerritory = !gOverlay.showTerritory; }
void toggle_border_overlay() { gOverlay.showBorders = !gOverlay.showBorders; }
void toggle_fog_overlay() { gOverlay.showFog = !gOverlay.showFog; }

} // namespace dom::render
