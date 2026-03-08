#include "engine/render/renderer.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <chrono>
#include "engine/ui/ui_icons.h"

namespace dom::render {
namespace {
struct OverlayState {
  bool showTerritory{true};
  bool showBorders{true};
  bool showFog{true};
  bool showTerrainMaterialOverlay{false};
  bool showWaterOverlay{false};
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
  int windowW{1400};
  int windowH{900};
  float renderScale{1.0f};
  float uiScale{1.0f};
  GLuint sceneFbo{0};
  GLuint sceneColorTex{0};
  int sceneW{0};
  int sceneH{0};
};

OverlayState gOverlay;
double gLastDrawMs = 0.0;
EditorPreview gEditorPreview{};

struct VisualFeedbackState {
  bool enabled{true};
  bool overlayDebug{false};
};

VisualFeedbackState gFeedbackState{};
VisualFeedbackCounters gFeedbackCounters{};

void draw_ring(glm::vec2 pos, float radius, float thickness, const std::array<float, 3>& color);

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

std::array<float,3> unit_color(const dom::sim::Unit& u) {
  auto tc = kTeamColors[std::min<size_t>(u.team + 1, 3)];
  if (u.supplyState == dom::sim::SupplyState::LowSupply) return {tc[0], tc[1] * 0.75f, tc[2] * 0.55f};
  if (u.supplyState == dom::sim::SupplyState::OutOfSupply) return {0.95f, 0.2f, 0.2f};
  return tc;
}

EntityPresentationCounters gEntityCounters{};
bool gEntityPresentationDebug{false};

std::array<float, 3> mix_color(const std::array<float, 3>& a, const std::array<float, 3>& b, float t) {
  float k = std::clamp(t, 0.0f, 1.0f);
  return {a[0] * (1.0f - k) + b[0] * k, a[1] * (1.0f - k) + b[1] * k, a[2] * (1.0f - k) + b[2] * k};
}

std::array<float, 3> theme_tint_for_team(const dom::sim::World& w, uint16_t team) {
  if (team >= w.players.size()) return {0.6f, 0.6f, 0.6f};
  std::string theme = w.players[team].civilization.themeId;
  if (theme.empty()) theme = w.players[team].civilization.id;
  if (theme.find("rome") != std::string::npos) return {0.78f, 0.62f, 0.42f};
  if (theme.find("china") != std::string::npos) return {0.56f, 0.44f, 0.32f};
  if (theme.find("russia") != std::string::npos) return {0.62f, 0.62f, 0.68f};
  if (theme.find("japan") != std::string::npos) return {0.84f, 0.72f, 0.74f};
  if (theme.find("middle") != std::string::npos || theme.find("egypt") != std::string::npos) return {0.76f, 0.66f, 0.48f};
  if (theme.find("usa") != std::string::npos || theme.find("uk") != std::string::npos || theme.find("europe") != std::string::npos || theme.find("eu") != std::string::npos) return {0.55f, 0.62f, 0.72f};
  return {0.64f, 0.64f, 0.64f};
}

enum class UnitGlyph : uint8_t { Worker, Infantry, HeavyInfantry, Cavalry, Artillery, Armor, Rail, Naval, Aircraft, Guardian };

UnitGlyph unit_glyph(const dom::sim::Unit& u) {
  using UT = dom::sim::UnitType;
  using UR = dom::sim::UnitRole;
  if (u.definitionId.find("guardian") != std::string::npos) return UnitGlyph::Guardian;
  if (u.definitionId.find("tank") != std::string::npos || u.definitionId.find("mech") != std::string::npos) return UnitGlyph::Armor;
  if (u.definitionId.find("heavy") != std::string::npos || u.definitionId.find("legion") != std::string::npos) return UnitGlyph::HeavyInfantry;
  if (u.definitionId.find("artillery") != std::string::npos) return UnitGlyph::Artillery;
  if (u.role == UR::Worker || u.type == UT::Worker) return UnitGlyph::Worker;
  if (u.role == UR::Naval || u.type == UT::TransportShip || u.type == UT::LightWarship || u.type == UT::HeavyWarship || u.type == UT::BombardShip) return UnitGlyph::Naval;
  if (u.type == UT::Fighter || u.type == UT::Interceptor || u.type == UT::Bomber || u.type == UT::StrategicBomber || u.type == UT::ReconDrone || u.type == UT::StrikeDrone || u.type == UT::TacticalMissile || u.type == UT::StrategicMissile) return UnitGlyph::Aircraft;
  if (u.role == UR::Siege || u.type == UT::Siege) return UnitGlyph::Artillery;
  if (u.role == UR::Cavalry || u.type == UT::Cavalry) return UnitGlyph::Cavalry;
  if (u.role == UR::Transport) return UnitGlyph::Rail;
  return UnitGlyph::Infantry;
}

std::array<float, 3> diplomatic_color(const dom::sim::World& w, uint16_t team) {
  auto base = kTeamColors[std::min<size_t>(team + 1, 3)];
  if (team == 0) return {base[0], base[1], base[2]};
  if (team < w.players.size()) {
    if (dom::sim::players_allied(w, 0, team)) return mix_color(base, {0.35f, 0.95f, 0.75f}, 0.4f);
    if (dom::sim::players_at_war(w, 0, team)) return mix_color(base, {1.0f, 0.2f, 0.2f}, 0.35f);
  }
  return mix_color(base, {0.85f, 0.85f, 0.85f}, 0.2f);
}


float tick_phase(const dom::sim::World& w, uint32_t stableId, float speed, float offset = 0.0f) {
  float t = static_cast<float>((w.tick + stableId) % 4096u);
  return std::fmod(t * speed + offset, 6.2831853f);
}

void draw_pulse_ring(glm::vec2 pos, float baseRadius, float pulseAmp, float thickness, float phase, const std::array<float, 3>& color) {
  float radius = baseRadius + std::sin(phase) * pulseAmp;
  draw_ring(pos, std::max(0.02f, radius), thickness, color);
}

void draw_deterministic_feedback(const dom::sim::World& w, const Camera& c, const std::unordered_set<uint32_t>& dragSet) {
  if (!gFeedbackState.enabled) return;

  for (const auto& u : w.units) {
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
    const bool combatActive = u.targetUnit != 0 || u.attackCooldownTicks > 0;
    if (combatActive) {
      ++gFeedbackCounters.combatEffectSpawns;
      uint32_t sid = u.id * 17u + static_cast<uint32_t>(u.type) * 131u;
      float phase = tick_phase(w, sid, 0.18f);
      float spark = 0.45f + std::sin(phase) * 0.2f;
      auto ccol = unit_color(u);
      glColor4f(std::min(1.0f, ccol[0] + spark), std::min(1.0f, ccol[1] + spark), std::min(1.0f, ccol[2] + spark), 0.58f);
      glBegin(GL_LINES);
      glVertex2f(u.renderPos.x, u.renderPos.y);
      glm::vec2 aim = u.target;
      if (u.targetUnit != 0) {
        for (const auto& enemy : w.units) if (enemy.id == u.targetUnit) { aim = enemy.renderPos; break; }
      }
      glVertex2f(u.renderPos.x + (aim.x - u.renderPos.x) * 0.45f, u.renderPos.y + (aim.y - u.renderPos.y) * 0.45f);
      glEnd();
      draw_pulse_ring(u.renderPos, 0.30f, 0.05f, 0.08f, phase, {1.0f, 0.82f, 0.34f});
    }

    if (u.selected) {
      ++gFeedbackCounters.selectionFeedbackEvents;
      float sphase = tick_phase(w, u.id * 43u, 0.12f, 1.2f);
      draw_pulse_ring(u.renderPos, 0.88f, 0.08f, 0.11f, sphase, {1.0f, 0.97f, 0.38f});
    } else if (dragSet.contains(u.id)) {
      ++gFeedbackCounters.selectionFeedbackEvents;
      draw_pulse_ring(u.renderPos, 0.74f, 0.04f, 0.08f, tick_phase(w, u.id * 11u, 0.08f), {0.82f, 0.82f, 0.82f});
    }
  }

  for (const auto& b : w.buildings) {
    if (b.factory.active || b.factory.blocked) {
      ++gFeedbackCounters.industryActivityEffects;
      float phase = tick_phase(w, b.id * 29u, 0.09f);
      auto col = b.factory.blocked ? std::array<float,3>{0.95f, 0.34f, 0.24f} : std::array<float,3>{0.98f, 0.84f, 0.32f};
      draw_pulse_ring(b.pos, 0.95f, 0.06f, 0.08f, phase, col);
      if (b.factory.active) {
        glColor4f(0.95f, 0.95f, 0.75f, 0.35f);
        glBegin(GL_POINTS);
        glVertex2f(b.pos.x, b.pos.y + 0.75f + std::sin(phase) * 0.15f);
        glEnd();
      }
    }
  }

  for (const auto& t : w.trains) {
    if (t.state == dom::sim::TrainState::Inactive || t.route.empty()) continue;
    auto it = std::find_if(w.railNodes.begin(), w.railNodes.end(), [&](const dom::sim::RailNode& n){ return n.id == t.currentNode; });
    if (it == w.railNodes.end()) { ++gFeedbackCounters.feedbackFallbackCount; continue; }
    ++gFeedbackCounters.industryActivityEffects;
    float phase = tick_phase(w, t.id * 7u, 0.15f);
    glm::vec2 tp{it->tile.x + 0.5f, it->tile.y + 0.5f};
    draw_pulse_ring(tp, 0.34f, 0.05f, 0.07f, phase, {0.75f, 0.95f, 1.0f});
  }

  for (const auto& ss : w.strategicStrikes) {
    if (ss.resolved) continue;
    ++gFeedbackCounters.strategicEffectSpawns;
    float phase = tick_phase(w, ss.id * 101u + static_cast<uint32_t>(ss.team), 0.06f);
    draw_pulse_ring(ss.target, 1.1f, 0.22f, 0.12f, phase, ss.warningIssued ? std::array<float,3>{1.0f, 0.3f, 0.3f} : std::array<float,3>{1.0f, 0.62f, 0.22f});
    glColor4f(1.0f, 0.35f, 0.28f, 0.45f);
    glBegin(GL_LINES);
    glVertex2f(ss.from.x, ss.from.y);
    glVertex2f(ss.target.x, ss.target.y);
    glEnd();
  }

  for (const auto& dz : w.denialZones) {
    ++gFeedbackCounters.strategicEffectSpawns;
    float phase = tick_phase(w, dz.id * 13u, 0.05f);
    draw_pulse_ring(dz.pos, dz.radius + 0.18f, 0.16f, 0.08f, phase, {0.95f, 0.30f, 0.34f});
  }

  for (const auto& ev : w.worldEvents) {
    if (ev.state == dom::sim::WorldEventState::Inactive) continue;
    ++gFeedbackCounters.crisisEffectSpawns;
  }

  for (const auto& s : w.guardianSites) {
    if (!s.discovered && !w.godMode) continue;
    ++gFeedbackCounters.guardianEffectSpawns;
    float phase = tick_phase(w, s.instanceId * 19u, 0.08f);
    auto col = (s.spawned && s.alive) ? std::array<float,3>{0.96f, 0.45f, 0.30f} : std::array<float,3>{0.76f, 0.30f, 0.92f};
    draw_pulse_ring(s.pos, 0.44f, 0.08f, 0.09f, phase, col);
  }

  if (w.armageddonActive) {
    ++gFeedbackCounters.strategicEffectSpawns;
    float phase = tick_phase(w, 77u, 0.03f);
    float pulse = 0.15f + (std::sin(phase) * 0.5f + 0.5f) * 0.15f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.95f, 0.15f, 0.25f, pulse);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(w.width), 0.0f);
    glVertex2f(static_cast<float>(w.width), static_cast<float>(w.height));
    glVertex2f(0.0f, static_cast<float>(w.height));
    glEnd();
    glDisable(GL_BLEND);
  }

  if (gFeedbackState.overlayDebug) {
    glColor3f(0.28f, 0.95f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(2.0f, 6.1f);
    glVertex2f(10.2f, 6.1f);
    glVertex2f(10.2f, 10.6f);
    glVertex2f(2.0f, 10.6f);
    glEnd();
  }
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


void draw_feature_circle(glm::vec2 pos, float radius, const std::array<float, 3>& color) {
  glColor3f(color[0], color[1], color[2]);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(pos.x, pos.y);
  for (int i = 0; i <= 16; ++i) {
    float a = static_cast<float>(i) * 0.39269908f;
    glVertex2f(pos.x + std::cos(a) * radius, pos.y + std::sin(a) * radius);
  }
  glEnd();
}

void draw_forest_and_feature_markers(const dom::sim::World& w, const Camera& c) {
  const bool detailed = c.zoom < 26.0f;
  const bool strategic = c.zoom > 70.0f;
  uint64_t forestCount = 0;
  if (detailed) {
    glBegin(GL_TRIANGLES);
    for (int y = 0; y < w.height - 1; ++y) {
      for (int x = 0; x < w.width - 1; ++x) {
        int i = y * w.width + x;
        auto sample = resolve_terrain_visual(w, i);
        if (!sample.hasForestCanopy || sample.isWater) continue;
        ++forestCount;
        float jitter = ((x * 73856093u) ^ (y * 19349663u)) & 1023u;
        float ox = (jitter / 1023.0f - 0.5f) * 0.28f;
        float oy = (((jitter * 37.0f) / 1023.0f) - 0.5f) * 0.28f;
        float cx = x + 0.5f + ox;
        float cy = y + 0.56f + oy;
        float s = 0.18f + (jitter / 1023.0f) * 0.08f;
        glColor3f(0.10f, 0.33f, 0.14f);
        glVertex2f(cx, cy + s);
        glVertex2f(cx - s, cy - s);
        glVertex2f(cx + s, cy - s);
      }
    }
    glEnd();
  } else {
    glBegin(GL_POINTS);
    glPointSize(strategic ? 2.5f : 3.5f);
    for (int y = 0; y < w.height; y += strategic ? 4 : 2) {
      for (int x = 0; x < w.width; x += strategic ? 4 : 2) {
        int i = y * w.width + x;
        auto sample = resolve_terrain_visual(w, i);
        if (!sample.hasForestCanopy || sample.isWater) continue;
        ++forestCount;
        glColor3f(0.14f, 0.36f, 0.18f);
        glVertex2f(x + 0.5f, y + 0.5f);
      }
    }
    glEnd();
  }
  add_forest_cluster_counter(forestCount);

  for (const auto& rn : w.resourceNodes) {
    if (!w.godMode) {
      int gx = std::clamp(static_cast<int>(rn.pos.x), 0, w.width - 1);
      int gy = std::clamp(static_cast<int>(rn.pos.y), 0, w.height - 1);
      if (w.fog[static_cast<size_t>(gy * w.width + gx)] > 0) continue;
    }
    std::array<float, 3> col{0.85f, 0.8f, 0.35f};
    if (rn.type == dom::sim::ResourceNodeType::Forest) col = {0.14f, 0.45f, 0.20f};
    else if (rn.type == dom::sim::ResourceNodeType::Ore) col = {0.78f, 0.78f, 0.84f};
    else if (rn.type == dom::sim::ResourceNodeType::Farmable) col = {0.85f, 0.72f, 0.34f};
    else if (rn.type == dom::sim::ResourceNodeType::Ruins) col = {0.66f, 0.56f, 0.66f};
    draw_feature_circle(rn.pos, strategic ? 0.16f : 0.24f, col);
  }

  for (const auto& dd : w.deepDeposits) {
    if (!dd.active) continue;
    int x = dd.cell % w.width;
    int y = dd.cell / w.width;
    glm::vec2 p{x + 0.5f, y + 0.5f};
    if (strategic) draw_feature_circle(p, 0.2f, {0.96f, 0.92f, 0.45f});
    else {
      glColor3f(0.96f, 0.92f, 0.45f);
      glBegin(GL_QUADS);
      glVertex2f(p.x - 0.22f, p.y - 0.22f);
      glVertex2f(p.x + 0.22f, p.y - 0.22f);
      glVertex2f(p.x + 0.22f, p.y + 0.22f);
      glVertex2f(p.x - 0.22f, p.y + 0.22f);
      glEnd();
    }
  }

  for (const auto& s : w.guardianSites) {
    ++gEntityCounters.guardianPresentationResolves;
    auto gPres = dom::sim::guardian_content_presentation(s.guardianId, s.siteType);
    if (gPres.iconId.find("fallback") != std::string::npos) ++gEntityCounters.entityPresentationFallbacks;
    if (!s.discovered && !w.godMode) continue;
    std::array<float, 3> col{0.77f, 0.22f, 0.82f};
    if (s.spawned && s.alive) col = {0.92f, 0.42f, 0.26f};
    draw_feature_circle(s.pos, strategic ? 0.26f : 0.34f, col);
  }
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
      auto sample = resolve_terrain_visual(w, static_cast<int>(gi));
      float r = sample.color.r;
      float g = sample.color.g;
      float b = sample.color.b;

      uint16_t owner = w.territoryOwner[gi];
      if (owner > 0) {
        auto team = team_rgb(static_cast<uint16_t>(owner - 1));
        r = std::clamp(r * 0.7f + (team[0] / 255.0f) * 0.3f, 0.0f, 1.0f);
        g = std::clamp(g * 0.7f + (team[1] / 255.0f) * 0.3f, 0.0f, 1.0f);
        b = std::clamp(b * 0.7f + (team[2] / 255.0f) * 0.3f, 0.0f, 1.0f);
      }

      if (!w.godMode && w.fog[gi] > 0) {
        const float fogMul = w.fog[gi] >= 200 ? 0.16f : 0.45f;
        r *= fogMul;
        g *= fogMul;
        b *= fogMul + 0.04f;
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
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
    int px = world_to_minimap_px(u.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(u.pos.y, static_cast<float>(w.height), res);
    plot_dot(out, res, px, py, team_rgb(u.team), 0);
  }
}


void ensure_scene_target(int width, int height) {
  int rw = std::max(1, (int)std::round(width * gOverlay.renderScale));
  int rh = std::max(1, (int)std::round(height * gOverlay.renderScale));
  if (gOverlay.sceneW == rw && gOverlay.sceneH == rh && gOverlay.sceneFbo != 0) return;
  gOverlay.sceneW = rw; gOverlay.sceneH = rh;
  if (gOverlay.sceneFbo == 0) glGenFramebuffers(1, &gOverlay.sceneFbo);
  if (gOverlay.sceneColorTex == 0) glGenTextures(1, &gOverlay.sceneColorTex);
  glBindTexture(GL_TEXTURE_2D, gOverlay.sceneColorTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rw, rh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, gOverlay.sceneFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gOverlay.sceneColorTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

void set_resolution(int width, int height) {
  gOverlay.windowW = std::max(1, width);
  gOverlay.windowH = std::max(1, height);
  glViewport(0, 0, gOverlay.windowW, gOverlay.windowH);
}

void set_render_scale(float scale) { gOverlay.renderScale = std::clamp(scale, 0.5f, 1.0f); }
void set_ui_scale(float scale) { gOverlay.uiScale = std::clamp(scale, 0.75f, 2.5f); }

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
  const int size = std::max(140, (int)std::round(210.0f * gOverlay.uiScale));
  const int pad = std::max(8, (int)std::round(18.0f * gOverlay.uiScale));
  int x0 = width - size - pad;
  int y0 = height - size - pad;
  if (!gOverlay.showMinimap) return false;
  if (screen.x < x0 || screen.y < y0 || screen.x > x0 + size || screen.y > y0 + size) return false;
  float nx = std::clamp((screen.x - x0) / static_cast<float>(size), 0.0f, 1.0f);
  float ny = std::clamp((screen.y - y0) / static_cast<float>(size), 0.0f, 1.0f);
  const float wx = std::clamp(nx * world.width, 0.0f, static_cast<float>(world.width));
  const float wy = std::clamp((1.0f - ny) * world.height, 0.0f, static_cast<float>(world.height));
  outWorld = {wx, wy};
  return true;
}

void draw(dom::sim::World& w, const Camera& c, int width, int height, const std::vector<uint32_t>& dragHighlight) {
  using Clock = std::chrono::steady_clock;
  const auto drawStart = Clock::now();
  set_resolution(width, height);
  ensure_scene_target(width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, gOverlay.sceneFbo);
  glViewport(0, 0, gOverlay.sceneW, gOverlay.sceneH);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  glOrtho(c.center.x - c.zoom * aspect, c.center.x + c.zoom * aspect, c.center.y - c.zoom, c.center.y + c.zoom, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  reset_terrain_presentation_counters();
  gEntityCounters = {};
  glBegin(GL_QUADS);
  for (int y = 0; y < w.height - 1; ++y) {
    for (int x = 0; x < w.width - 1; ++x) {
      size_t i = y * w.width + x;
      auto sample = resolve_terrain_visual(w, static_cast<int>(i));
      glColor3f(sample.color.r, sample.color.g, sample.color.b);
      glVertex2f(x, y); glVertex2f(x + 1, y); glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
    }
  }
  glEnd();

  glBegin(GL_LINES);
  for (int y = 1; y < w.height - 1; ++y) {
    for (int x = 1; x < w.width - 1; ++x) {
      size_t i = y * w.width + x;
      auto sample = resolve_terrain_visual(w, static_cast<int>(i));
      if (!sample.hasCliff) continue;
      float slope = terrain_slope_hint(w, static_cast<int>(i));
      float len = 0.18f + slope * 0.24f;
      glColor3f(0.22f, 0.20f, 0.18f);
      glVertex2f(x + 0.5f - len, y + 0.5f - len);
      glVertex2f(x + 0.5f + len, y + 0.5f + len);
    }
  }
  glEnd();

  if (gOverlay.showTerrainMaterialOverlay || gOverlay.showWaterOverlay) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (int y = 0; y < w.height - 1; ++y) {
      for (int x = 0; x < w.width - 1; ++x) {
        size_t i = y * w.width + x;
        auto sample = resolve_terrain_visual(w, static_cast<int>(i));
        if (gOverlay.showWaterOverlay && !sample.isWater) continue;
        if (gOverlay.showTerrainMaterialOverlay && !gOverlay.showWaterOverlay && sample.isWater) continue;
        glm::vec3 oc = gOverlay.showWaterOverlay ? glm::vec3(0.24f, 0.72f, 1.0f) : sample.accent;
        glColor4f(oc.r, oc.g, oc.b, 0.20f);
        glVertex2f(x, y); glVertex2f(x + 1, y); glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
      }
    }
    glEnd();
    glDisable(GL_BLEND);
  }

  update_overlay_textures(w);
  if (gOverlay.showTerritory) draw_textured_overlay(gOverlay.territoryTex, w, 0.20f, {0.55f, 0.55f, 0.95f}, true);
  if (gOverlay.showBorders) draw_textured_overlay(gOverlay.borderTex, w, 0.45f, {0.98f, 0.95f, 0.5f}, true);
  if (gOverlay.showFog && !w.godMode) draw_textured_overlay(gOverlay.fogTex, w, 1.0f, {0.0f, 0.0f, 0.0f}, true);

  draw_forest_and_feature_markers(w, c);

  glLineWidth(c.zoom > 30.0f ? 1.0f : 2.0f);
  glBegin(GL_LINES);
  for (const auto& r : w.roads) {
    if (r.owner == 0) glColor3f(0.95f, 0.85f, 0.35f);
    else if (r.owner == 1) glColor3f(0.35f, 0.8f, 0.95f);
    else glColor3f(0.7f, 0.7f, 0.7f);
    glVertex2f((float)r.a.x + 0.5f, (float)r.a.y + 0.5f);
    glVertex2f((float)r.b.x + 0.5f, (float)r.b.y + 0.5f);
  }
  glEnd();

  glLineWidth(c.zoom > 30.0f ? 1.2f : 2.6f);
  glBegin(GL_LINES);
  for (const auto& e : w.railEdges) {
    glm::vec2 a{0.0f}, b{0.0f};
    for (const auto& n : w.railNodes) {
      if (n.id == e.aNode) a = {n.tile.x + 0.5f, n.tile.y + 0.5f};
      if (n.id == e.bNode) b = {n.tile.x + 0.5f, n.tile.y + 0.5f};
    }
    if (e.disrupted) glColor3f(0.92f, 0.28f, 0.22f);
    else if (e.bridge) glColor3f(0.86f, 0.86f, 0.52f);
    else glColor3f(0.74f, 0.74f, 0.78f);
    glVertex2f(a.x, a.y); glVertex2f(b.x, b.y);
  }
  glEnd();

  glPointSize(c.zoom > 40.0f ? 4.0f : 6.0f);
  glBegin(GL_POINTS);
  for (const auto& n : w.railNodes) {
    if (!n.active) glColor3f(0.4f, 0.4f, 0.4f);
    else if (n.type == dom::sim::RailNodeType::Station || n.type == dom::sim::RailNodeType::Depot) glColor3f(0.97f, 0.92f, 0.42f);
    else glColor3f(0.78f, 0.78f, 0.84f);
    glVertex2f(n.tile.x + 0.5f, n.tile.y + 0.5f);
  }
  glEnd();

  glBegin(GL_POINTS);
  glPointSize(c.zoom > 30.0f ? 3.0f : 5.0f);
  for (const auto& t : w.trains) {
    if (t.state != dom::sim::TrainState::Active) glColor3f(0.45f, 0.45f, 0.45f);
    else if (t.type == dom::sim::TrainType::Supply) glColor3f(0.35f, 0.95f, 0.35f);
    else if (t.type == dom::sim::TrainType::Freight) glColor3f(0.95f, 0.75f, 0.2f);
    else glColor3f(0.85f, 0.25f, 0.25f);
    glm::vec2 p{0.0f, 0.0f};
    bool found = false;
    for (const auto& n : w.railNodes) if (n.id == t.currentNode) { p = {n.tile.x + 0.5f, n.tile.y + 0.5f}; found = true; break; }
    if (!found && t.currentEdge != 0) {
      const dom::sim::RailEdge* e = nullptr;
      for (const auto& re : w.railEdges) if (re.id == t.currentEdge) { e = &re; break; }
      if (e) {
        glm::vec2 a{0.0f, 0.0f}, b{0.0f, 0.0f};
        for (const auto& n : w.railNodes) { if (n.id == e->aNode) a = {n.tile.x + 0.5f, n.tile.y + 0.5f}; if (n.id == e->bNode) b = {n.tile.x + 0.5f, n.tile.y + 0.5f}; }
        p = glm::mix(a, b, std::clamp(t.segmentProgress, 0.0f, 1.0f));
      }
    }
    glVertex2f(p.x, p.y);
  }
  glEnd();

  glLineWidth(1.0f);
  glBegin(GL_LINES);
  for (const auto& tr : w.tradeRoutes) {
    if (!tr.active) continue;
    const dom::sim::City* a = nullptr;
    const dom::sim::City* b = nullptr;
    for (const auto& c2 : w.cities) { if (c2.id == tr.fromCity) a = &c2; if (c2.id == tr.toCity) b = &c2; }
    if (!a || !b) continue;
    glColor3f(0.95f, 0.95f, 0.2f);
    glVertex2f(a->pos.x, a->pos.y);
    glVertex2f(b->pos.x, b->pos.y);
  }
  glEnd();

  glBegin(GL_QUADS);
  for (const auto& b : w.buildings) {
    ++gEntityCounters.buildingPresentationResolves;
    auto bPres = dom::sim::building_content_presentation(w, b.team, b.type, b.definitionId);
    if (bPres.iconId.find("fallback") != std::string::npos) ++gEntityCounters.entityPresentationFallbacks;
    auto rel = diplomatic_color(w, b.team);
    auto theme = theme_tint_for_team(w, b.team);
    auto base = mix_color(rel, theme, 0.35f);
    if (b.underConstruction) base = mix_color(base, {0.35f, 0.35f, 0.35f}, 0.45f);
    if (b.factory.blocked) base = {0.88f, 0.35f, 0.25f};
    else if (b.factory.active) base = mix_color(base, {0.95f, 0.95f, 0.42f}, 0.2f);
    float sx = b.size.x * 0.5f;
    float sy = b.size.y * 0.5f;
    const bool military = b.type == dom::sim::BuildingType::Barracks || b.type == dom::sim::BuildingType::AABattery || b.type == dom::sim::BuildingType::AntiMissileDefense || b.type == dom::sim::BuildingType::Airbase || b.type == dom::sim::BuildingType::MissileSilo;
    const bool logistics = b.type == dom::sim::BuildingType::Port || b.type == dom::sim::BuildingType::Market;
    const bool industrial = b.type == dom::sim::BuildingType::SteelMill || b.type == dom::sim::BuildingType::Refinery || b.type == dom::sim::BuildingType::MachineWorks || b.type == dom::sim::BuildingType::FactoryHub || b.type == dom::sim::BuildingType::MunitionsPlant || b.type == dom::sim::BuildingType::ElectronicsLab || b.type == dom::sim::BuildingType::Mine;
    if (military) { sx *= 1.0f; sy *= 0.8f; }
    else if (logistics) { sx *= 1.2f; sy *= 0.75f; }
    else if (industrial) { sx *= 1.1f; sy *= 1.05f; }
    glColor3f(base[0], base[1], base[2]);
    glVertex2f(b.pos.x - sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y + sy);
    glVertex2f(b.pos.x - sx, b.pos.y + sy);

    auto accent = mix_color(rel, {1.0f, 1.0f, 1.0f}, 0.2f);
    if (b.type == dom::sim::BuildingType::MissileSilo) accent = {0.95f, 0.24f, 0.24f};
    if (b.type == dom::sim::BuildingType::RadarTower || b.type == dom::sim::BuildingType::MobileRadar) accent = {0.35f, 0.95f, 0.95f};
    glColor3f(accent[0], accent[1], accent[2]);
    float ax = sx * 0.42f;
    float ay = sy * 0.42f;
    glVertex2f(b.pos.x - ax, b.pos.y - ay);
    glVertex2f(b.pos.x + ax, b.pos.y - ay);
    glVertex2f(b.pos.x + ax, b.pos.y + ay);
    glVertex2f(b.pos.x - ax, b.pos.y + ay);
  }
  glEnd();

  glPointSize(c.zoom > 30.0f ? 2.0f : 3.0f);
  glBegin(GL_POINTS);
  for (const auto& d : w.detectors) {
    if (!d.active) continue;
    glColor3f(0.35f, 0.95f, 0.95f);
    glVertex2f(d.pos.x, d.pos.y);
  }
  glEnd();

  glLineWidth(1.0f);
  for (const auto& dz : w.denialZones) {
    const int seg = 24;
    glColor3f(0.95f, 0.25f, 0.25f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; ++i) {
      float a = 6.2831853f * (float)i / (float)seg;
      glVertex2f(dz.pos.x + std::cos(a) * dz.radius, dz.pos.y + std::sin(a) * dz.radius);
    }
    glEnd();
  }

  glPointSize(c.zoom > 35.0f ? 3.0f : 5.0f);
  glBegin(GL_POINTS);
  for (const auto& ss : w.strategicStrikes) {
    if (ss.resolved) continue;
    glColor3f(ss.warningIssued ? 1.0f : 0.8f, ss.warningIssued ? 0.2f : 0.5f, 0.2f);
    glVertex2f(ss.target.x, ss.target.y);
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
      ++gEntityCounters.farLodClusterCount;
      auto tc = diplomatic_color(w, b.team);
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
      if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
      ++gEntityCounters.unitPresentationResolves;
      auto uPres = dom::sim::unit_content_presentation(w, u.team, u.type, u.definitionId);
      if (uPres.iconId.find("fallback") != std::string::npos) ++gEntityCounters.entityPresentationFallbacks;
      auto rel = unit_color(u);
      auto theme = theme_tint_for_team(w, u.team);
      auto base = mix_color(rel, theme, 0.25f);
      UnitGlyph glyph = unit_glyph(u);
      float s = c.zoom < nearThreshold ? 0.38f : (c.zoom < farThreshold ? 0.46f : 0.50f);
      if (glyph == UnitGlyph::Guardian) s += 0.16f;
      if (glyph == UnitGlyph::Armor || glyph == UnitGlyph::Naval) s += 0.08f;

      if (glyph == UnitGlyph::Aircraft) {
        glBegin(GL_TRIANGLES);
        glColor3f(base[0], base[1], base[2]);
        glVertex2f(u.renderPos.x, u.renderPos.y + s);
        glVertex2f(u.renderPos.x - s, u.renderPos.y - s * 0.7f);
        glVertex2f(u.renderPos.x + s, u.renderPos.y - s * 0.7f);
        glEnd();
      } else if (glyph == UnitGlyph::Naval) {
        glBegin(GL_QUADS);
        glColor3f(base[0], base[1], base[2]);
        glVertex2f(u.renderPos.x - s, u.renderPos.y - s * 0.55f);
        glVertex2f(u.renderPos.x + s, u.renderPos.y - s * 0.55f);
        glVertex2f(u.renderPos.x + s * 0.8f, u.renderPos.y + s * 0.55f);
        glVertex2f(u.renderPos.x - s * 0.8f, u.renderPos.y + s * 0.55f);
        glEnd();
      } else if (glyph == UnitGlyph::Artillery || glyph == UnitGlyph::Rail) {
        glBegin(GL_QUADS);
        glColor3f(base[0], base[1], base[2]);
        glVertex2f(u.renderPos.x - s, u.renderPos.y - s * 0.45f);
        glVertex2f(u.renderPos.x + s, u.renderPos.y - s * 0.45f);
        glVertex2f(u.renderPos.x + s, u.renderPos.y + s * 0.45f);
        glVertex2f(u.renderPos.x - s, u.renderPos.y + s * 0.45f);
        glEnd();
      } else {
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(base[0], base[1], base[2]);
        glVertex2f(u.renderPos.x, u.renderPos.y);
        for (int i = 0; i <= 10; ++i) {
          float a = i * 0.62831853f;
          float rr = s;
          if (glyph == UnitGlyph::Worker) rr *= 0.8f;
          if (glyph == UnitGlyph::HeavyInfantry) rr *= 1.12f;
          if (glyph == UnitGlyph::Cavalry) rr *= (i % 2 == 0 ? 1.05f : 0.82f);
          glVertex2f(u.renderPos.x + std::cos(a) * rr, u.renderPos.y + std::sin(a) * rr);
        }
        glEnd();
      }

      glBegin(GL_QUADS);
      auto accent = mix_color(rel, {1.0f, 1.0f, 1.0f}, 0.35f);
      glColor3f(accent[0], accent[1], accent[2]);
      float badge = s * 0.3f;
      glVertex2f(u.renderPos.x - badge, u.renderPos.y - badge);
      glVertex2f(u.renderPos.x + badge, u.renderPos.y - badge);
      glVertex2f(u.renderPos.x + badge, u.renderPos.y + badge);
      glVertex2f(u.renderPos.x - badge, u.renderPos.y + badge);
      glEnd();
    }
  }

  std::unordered_set<uint32_t> dragSet(dragHighlight.begin(), dragHighlight.end());
  for (const auto& u : w.units) {
    if (u.selected) draw_ring(u.renderPos, 0.86f, 0.19f, {1.0f, 0.95f, 0.25f});
    else if (dragSet.contains(u.id)) draw_ring(u.renderPos, 0.70f, 0.13f, {0.85f, 0.85f, 0.85f});
    if (u.supplyState == dom::sim::SupplyState::LowSupply) { (void)dom::ui::icons::resolve_marker_id("warning", u.team); draw_ring(u.renderPos, 0.54f, 0.09f, {0.95f, 0.65f, 0.22f}); }
    if (u.supplyState == dom::sim::SupplyState::OutOfSupply) { (void)dom::ui::icons::resolve_marker_id("warning", u.team); draw_ring(u.renderPos, 0.60f, 0.10f, {0.95f, 0.22f, 0.22f}); }
  }

  for (const auto& cty : w.cities) {
    int gx = std::clamp(static_cast<int>(cty.pos.x), 0, w.width - 1);
    int gy = std::clamp(static_cast<int>(cty.pos.y), 0, w.height - 1);
    if (!w.godMode && w.fog[gy * w.width + gx] > 0 && cty.team != 0) continue;
    ++gEntityCounters.cityPresentationResolves;
    auto rel = diplomatic_color(w, cty.team);
    auto theme = theme_tint_for_team(w, cty.team);
    auto col = mix_color(rel, theme, 0.3f);
    float core = cty.capital ? 1.25f : (cty.level >= 3 ? 1.05f : 0.82f);
    if (c.zoom > 80.0f) core *= 0.82f;
    glBegin(GL_QUADS);
    glColor3f(col[0], col[1], col[2]);
    glVertex2f(cty.pos.x - core, cty.pos.y - core * 0.9f);
    glVertex2f(cty.pos.x + core, cty.pos.y - core * 0.9f);
    glVertex2f(cty.pos.x + core, cty.pos.y + core * 0.9f);
    glVertex2f(cty.pos.x - core, cty.pos.y + core * 0.9f);
    glEnd();

    glBegin(GL_TRIANGLES);
    auto lm = cty.capital ? std::array<float,3>{1.0f, 0.96f, 0.65f} : std::array<float,3>{0.9f, 0.9f, 0.9f};
    glColor3f(lm[0], lm[1], lm[2]);
    float h = cty.capital ? 0.95f : 0.7f;
    glVertex2f(cty.pos.x, cty.pos.y + h);
    glVertex2f(cty.pos.x - h * 0.55f, cty.pos.y + 0.15f);
    glVertex2f(cty.pos.x + h * 0.55f, cty.pos.y + 0.15f);
    glEnd();

    if (cty.capital || cty.level >= 4) { (void)dom::ui::icons::resolve_marker_id("capital", cty.team); draw_ring(cty.pos, core + 0.42f, 0.09f, {0.95f, 0.88f, 0.3f}); }
  }

  if (gEntityPresentationDebug) {
    glLineWidth(1.0f);
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(1.2f, 1.2f);
    glVertex2f(7.2f, 1.2f);
    glVertex2f(7.2f, 5.6f);
    glVertex2f(1.2f, 5.6f);
    glEnd();
  }

  if (gEditorPreview.enabled) {

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float r = gEditorPreview.r;
    float g = gEditorPreview.g;
    float b = gEditorPreview.b;
    if (!gEditorPreview.valid) { r = 0.95f; g = 0.25f; b = 0.2f; }
    glColor4f(r, g, b, gEditorPreview.alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(gEditorPreview.pos.x, gEditorPreview.pos.y);
    for (int i = 0; i <= 32; ++i) {
      float a = (float)i / 32.0f * 6.2831853f;
      glVertex2f(gEditorPreview.pos.x + std::cos(a) * gEditorPreview.radius, gEditorPreview.pos.y + std::sin(a) * gEditorPreview.radius);
    }
    glEnd();
    glDisable(GL_BLEND);
  }

  draw_deterministic_feedback(w, c, dragSet);

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

    const float size = std::max(140.0f, 210.0f * gOverlay.uiScale);
    const float pad = std::max(8.0f, 18.0f * gOverlay.uiScale);
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.98f, 0.95f, 0.65f, 0.14f);
    glBegin(GL_QUADS);
    glVertex2f(vx0, vy0); glVertex2f(vx1, vy0); glVertex2f(vx1, vy1); glVertex2f(vx0, vy1);
    glEnd();
    glLineWidth(2.0f);
    glColor3f(1.0f, 1.0f, 0.8f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(vx0, vy0); glVertex2f(vx1, vy0); glVertex2f(vx1, vy1); glVertex2f(vx0, vy1);
    glEnd();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, gOverlay.sceneFbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, gOverlay.sceneW, gOverlay.sceneH, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  const auto drawEnd = Clock::now();
  gLastDrawMs = std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();
}


void generate_minimap_image(const dom::sim::World& world, int resolution, std::vector<uint8_t>& outRgb) {
  int res = std::max(32, resolution);
  build_minimap_pixels(world, res, outRgb);
}
void toggle_minimap() { gOverlay.showMinimap = !gOverlay.showMinimap; }
void toggle_territory_overlay() { gOverlay.showTerritory = !gOverlay.showTerritory; }
void toggle_border_overlay() { gOverlay.showBorders = !gOverlay.showBorders; }
void toggle_fog_overlay() { gOverlay.showFog = !gOverlay.showFog; }
void toggle_terrain_material_overlay() { gOverlay.showTerrainMaterialOverlay = !gOverlay.showTerrainMaterialOverlay; }
void toggle_water_overlay() { gOverlay.showWaterOverlay = !gOverlay.showWaterOverlay; }
void set_entity_presentation_debug(bool enabled) { gEntityPresentationDebug = enabled; }
bool entity_presentation_debug() { return gEntityPresentationDebug; }
const EntityPresentationCounters& entity_presentation_counters() { return gEntityCounters; }
void set_visual_feedback_enabled(bool enabled) { gFeedbackState.enabled = enabled; }
bool visual_feedback_enabled() { return gFeedbackState.enabled; }
void set_visual_feedback_overlay_debug(bool enabled) { gFeedbackState.overlayDebug = enabled; }
bool visual_feedback_overlay_debug() { return gFeedbackState.overlayDebug; }
const VisualFeedbackCounters& visual_feedback_counters() { return gFeedbackCounters; }
double last_draw_ms() { return gLastDrawMs; }

void set_editor_preview(const EditorPreview& preview) { gEditorPreview = preview; }

} // namespace dom::render
