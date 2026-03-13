#include "engine/render/renderer.h"
#include "engine/render/content_resolution.h"
#include "engine/render/render_stylesheet.h"
#include "engine/render/terrain_chunk_mesh.h"
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
#include <cctype>
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
  bool showStrategicOverlays{true};
  bool showLabels{true};
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
StrategicVisualizationCounters gStrategicCounters{};

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

std::string normalized_civ_key(const dom::sim::World& w, uint16_t team) {
  if (team >= w.players.size()) return "default";
  std::string k = w.players[team].civilization.id;
  if (k.empty()) k = w.players[team].civilization.themeId;
  if (k.empty()) k = "default";
  std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return k;
}

std::array<float, 3> theme_tint_for_team(const dom::sim::World& w, uint16_t team) {
  std::string theme = normalized_civ_key(w, team);
  if (theme.find("rome") != std::string::npos) return {0.78f, 0.62f, 0.42f};
  if (theme.find("china") != std::string::npos) return {0.56f, 0.44f, 0.32f};
  if (theme.find("europe") != std::string::npos) return {0.52f, 0.62f, 0.76f};
  if (theme == "eu" || theme.find("european_union") != std::string::npos) return {0.42f, 0.5f, 0.84f};
  if (theme.find("uk") != std::string::npos || theme.find("brit") != std::string::npos) return {0.48f, 0.56f, 0.72f};
  if (theme.find("usa") != std::string::npos || theme.find("america") != std::string::npos) return {0.56f, 0.64f, 0.78f};
  if (theme.find("middle") != std::string::npos) return {0.78f, 0.66f, 0.44f};
  if (theme.find("egypt") != std::string::npos) return {0.82f, 0.72f, 0.48f};
  if (theme.find("russia") != std::string::npos) return {0.62f, 0.62f, 0.68f};
  if (theme.find("japan") != std::string::npos) return {0.84f, 0.72f, 0.74f};
  if (theme.find("tartaria") != std::string::npos) return {0.63f, 0.36f, 0.74f};
  return {0.64f, 0.64f, 0.64f};
}

enum class CivSettlementShape : uint8_t { Generic, Rome, China, Europe, MiddleEast, Russia, Usa, Japan, Eu, Uk, Egypt, Tartaria };

CivSettlementShape civ_settlement_shape(const dom::sim::World& w, uint16_t team) {
  std::string k = normalized_civ_key(w, team);
  if (k.find("rome") != std::string::npos) return CivSettlementShape::Rome;
  if (k.find("china") != std::string::npos) return CivSettlementShape::China;
  if (k.find("europe") != std::string::npos) return CivSettlementShape::Europe;
  if (k.find("middle") != std::string::npos) return CivSettlementShape::MiddleEast;
  if (k.find("russia") != std::string::npos) return CivSettlementShape::Russia;
  if (k.find("usa") != std::string::npos || k.find("america") != std::string::npos) return CivSettlementShape::Usa;
  if (k == "eu" || k.find("european_union") != std::string::npos) return CivSettlementShape::Eu;
  if (k.find("uk") != std::string::npos || k.find("brit") != std::string::npos) return CivSettlementShape::Uk;
  if (k.find("japan") != std::string::npos) return CivSettlementShape::Japan;
  if (k.find("egypt") != std::string::npos) return CivSettlementShape::Egypt;
  if (k.find("tartaria") != std::string::npos) return CivSettlementShape::Tartaria;
  return CivSettlementShape::Generic;
}

enum class UnitGlyph : uint8_t { Worker, Infantry, RangedInfantry, HeavyInfantry, Cavalry, Artillery, Armor, Rail, Naval, Aircraft, Guardian };

UnitGlyph unit_glyph(const dom::sim::Unit& u) {
  using UT = dom::sim::UnitType;
  using UR = dom::sim::UnitRole;
  if (u.definitionId.find("guardian") != std::string::npos) return UnitGlyph::Guardian;
  if (u.definitionId.find("archer") != std::string::npos || u.definitionId.find("ranged") != std::string::npos || u.definitionId.find("bow") != std::string::npos) return UnitGlyph::RangedInfantry;
  if (u.definitionId.find("raider") != std::string::npos || u.definitionId.find("scout") != std::string::npos) return UnitGlyph::Cavalry;
  if (u.definitionId.find("tank") != std::string::npos || u.definitionId.find("mech") != std::string::npos) return UnitGlyph::Armor;
  if (u.definitionId.find("heavy") != std::string::npos || u.definitionId.find("legion") != std::string::npos) return UnitGlyph::HeavyInfantry;
  if (u.definitionId.find("artillery") != std::string::npos) return UnitGlyph::Artillery;
  if (u.definitionId.find("train") != std::string::npos) return UnitGlyph::Rail;
  if (u.role == UR::Worker || u.type == UT::Worker) return UnitGlyph::Worker;
  if (u.role == UR::Naval || u.type == UT::TransportShip || u.type == UT::LightWarship || u.type == UT::HeavyWarship || u.type == UT::BombardShip) return UnitGlyph::Naval;
  if (u.type == UT::Fighter || u.type == UT::Interceptor || u.type == UT::Bomber || u.type == UT::StrategicBomber || u.type == UT::ReconDrone || u.type == UT::StrikeDrone || u.type == UT::TacticalMissile || u.type == UT::StrategicMissile) return UnitGlyph::Aircraft;
  if (u.role == UR::Siege || u.type == UT::Siege) return UnitGlyph::Artillery;
  if (u.role == UR::Cavalry || u.type == UT::Cavalry) return UnitGlyph::Cavalry;
  if (u.role == UR::Transport) return UnitGlyph::Rail;
  return UnitGlyph::Infantry;
}


std::string team_theme_id(const dom::sim::World& w, uint16_t team) {
  if (team >= w.players.size()) return "default";
  std::string theme = w.players[team].civilization.themeId;
  std::transform(theme.begin(), theme.end(), theme.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return theme.empty() ? "default" : theme;
}

std::string unit_render_class(const dom::sim::Unit& u) {
  switch (u.type) {
    case dom::sim::UnitType::Worker: return "worker";
    case dom::sim::UnitType::Infantry: return "infantry";
    case dom::sim::UnitType::Archer: return "infantry";
    case dom::sim::UnitType::Cavalry: return "cavalry";
    case dom::sim::UnitType::Siege: return "artillery";
    case dom::sim::UnitType::TransportShip: return "transport_ship";
    case dom::sim::UnitType::LightWarship:
    case dom::sim::UnitType::HeavyWarship:
    case dom::sim::UnitType::BombardShip: return "warship";
    case dom::sim::UnitType::Fighter:
    case dom::sim::UnitType::Interceptor: return "fighter";
    case dom::sim::UnitType::Bomber:
    case dom::sim::UnitType::StrategicBomber: return "bomber";
    case dom::sim::UnitType::TacticalMissile:
    case dom::sim::UnitType::StrategicMissile: return "missile";
    default: return "infantry";
  }
}

std::string building_render_class(dom::sim::BuildingType type) {
  switch (type) {
    case dom::sim::BuildingType::CityCenter: return "city_settlement";
    case dom::sim::BuildingType::Barracks: return "barracks";
    case dom::sim::BuildingType::Market: return "market";
    case dom::sim::BuildingType::FactoryHub: return "factory";
    case dom::sim::BuildingType::SteelMill:
    case dom::sim::BuildingType::Refinery:
    case dom::sim::BuildingType::MachineWorks:
    case dom::sim::BuildingType::MunitionsPlant:
    case dom::sim::BuildingType::ElectronicsLab: return "industrial_node";
    case dom::sim::BuildingType::MissileSilo: return "missile_silo";
    case dom::sim::BuildingType::RadarTower:
    case dom::sim::BuildingType::MobileRadar: return "radar";
    case dom::sim::BuildingType::Port: return "port";
    case dom::sim::BuildingType::Mine: return "mine_entrance";
    default: return "strategic_structure";
  }
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

std::array<float, 3> owner_tint_color(const dom::sim::World& w, uint16_t owner) {
  if (owner == 0) return {0.48f, 0.82f, 0.55f};
  const uint16_t team = static_cast<uint16_t>(owner - 1);
  auto dc = diplomatic_color(w, team);
  if (team < w.players.size()) {
    if (dom::sim::players_allied(w, 0, team)) return mix_color(dc, {0.45f, 0.95f, 0.72f}, 0.25f);
    if (dom::sim::players_at_war(w, 0, team)) return mix_color(dc, {1.0f, 0.26f, 0.2f}, 0.38f);
  }
  return mix_color(dc, {0.85f, 0.85f, 0.85f}, 0.24f);
}

bool tile_frontline(const dom::sim::World& w, int x, int y, uint16_t owner) {
  if (owner == 0) return false;
  const uint16_t team = static_cast<uint16_t>(owner - 1);
  const int r = 3;
  for (const auto& u : w.units) {
    int ux = static_cast<int>(u.pos.x);
    int uy = static_cast<int>(u.pos.y);
    if (std::abs(ux - x) > r || std::abs(uy - y) > r) continue;
    if (u.team == team) continue;
    if (dom::sim::players_at_war(w, team, u.team)) return true;
  }
  return false;
}

bool tile_recently_captured(const dom::sim::World& w, int x, int y, uint32_t recentTicks) {
  (void)w; (void)x; (void)y; (void)recentTicks;
  return false;
}


float tick_phase(const dom::sim::World& w, uint32_t stableId, float speed, float offset = 0.0f) {
  float t = static_cast<float>((w.tick + stableId) % 4096u);
  return std::fmod(t * speed + offset, 6.2831853f);
}

void draw_pulse_ring(glm::vec2 pos, float baseRadius, float pulseAmp, float thickness, float phase, const std::array<float, 3>& color) {
  float radius = baseRadius + std::sin(phase) * pulseAmp;
  draw_ring(pos, std::max(0.02f, radius), thickness, color);
}



glm::vec2 rail_node_pos(const dom::sim::World& w, uint32_t nodeId, bool& found) {
  found = false;
  for (const auto& n : w.railNodes) {
    if (n.id != nodeId) continue;
    found = true;
    return {n.tile.x + 0.5f, n.tile.y + 0.5f};
  }
  return {};
}

void draw_arrow_head(glm::vec2 from, glm::vec2 to, float size) {
  glm::vec2 dir = to - from;
  float len = glm::length(dir);
  if (len < 0.0001f) return;
  glm::vec2 n = dir / len;
  glm::vec2 side{-n.y, n.x};
  glm::vec2 tip = to;
  glm::vec2 back = to - n * size;
  glBegin(GL_TRIANGLES);
  glVertex2f(tip.x, tip.y);
  glVertex2f(back.x + side.x * size * 0.45f, back.y + side.y * size * 0.45f);
  glVertex2f(back.x - side.x * size * 0.45f, back.y - side.y * size * 0.45f);
  glEnd();
}


std::array<float, 3> army_stance_color(dom::sim::ArmyGroupStance stance) {
  switch (stance) {
    case dom::sim::ArmyGroupStance::Offensive: return {0.95f, 0.36f, 0.28f};
    case dom::sim::ArmyGroupStance::Defensive: return {0.38f, 0.82f, 0.96f};
  }
  return {0.85f, 0.85f, 0.85f};
}

void draw_unit_destination_marker(const dom::sim::World& w, const dom::sim::Unit& u, float scale) {
  auto glyph = unit_glyph(u);
  auto col = diplomatic_color(w, u.team);
  col = mix_color(col, {1.0f, 1.0f, 1.0f}, 0.28f);
  float s = std::max(0.24f, scale);
  glColor4f(col[0], col[1], col[2], 0.88f);
  if (glyph == UnitGlyph::Aircraft) {
    glBegin(GL_TRIANGLES);
    glVertex2f(u.target.x, u.target.y + s * 1.25f);
    glVertex2f(u.target.x - s, u.target.y - s * 0.75f);
    glVertex2f(u.target.x + s, u.target.y - s * 0.75f);
    glEnd();
    return;
  }
  if (glyph == UnitGlyph::Naval) {
    glBegin(GL_QUADS);
    glVertex2f(u.target.x - s * 1.2f, u.target.y - s * 0.7f);
    glVertex2f(u.target.x + s * 1.2f, u.target.y - s * 0.7f);
    glVertex2f(u.target.x + s * 0.8f, u.target.y + s * 0.7f);
    glVertex2f(u.target.x - s * 0.8f, u.target.y + s * 0.7f);
    glEnd();
    return;
  }
  glBegin(GL_QUADS);
  glVertex2f(u.target.x - s, u.target.y - s);
  glVertex2f(u.target.x + s, u.target.y - s);
  glVertex2f(u.target.x + s, u.target.y + s);
  glVertex2f(u.target.x - s, u.target.y + s);
  glEnd();
}

void draw_army_group_formations(const dom::sim::World& w, const Camera& c) {
  if (c.zoom > 55.0f) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto& group : w.armyGroups) {
    if (!group.active || group.unitIds.size() < 2) continue;
    std::vector<const dom::sim::Unit*> units;
    units.reserve(group.unitIds.size());
    for (uint32_t id : group.unitIds) {
      for (const auto& u : w.units) {
        if (u.id != id) continue;
        if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
        units.push_back(&u);
        break;
      }
    }
    if (units.size() < 2) {
      ++gStrategicCounters.visualFallbackCount;
      continue;
    }

    glm::vec2 center{0.0f, 0.0f};
    for (const auto* u : units) center += u->renderPos;
    center /= static_cast<float>(units.size());

    float avgRadius = 0.0f;
    for (const auto* u : units) avgRadius += glm::length(u->renderPos - center);
    avgRadius = std::max(0.7f, avgRadius / static_cast<float>(units.size()) + 0.45f);

    auto stanceCol = army_stance_color(group.stance);
    auto teamCol = diplomatic_color(w, group.owner);
    auto col = mix_color(stanceCol, teamCol, 0.45f);

    glLineWidth(c.zoom > 28.0f ? 1.0f : 1.8f);
    glColor4f(col[0], col[1], col[2], 0.55f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; ++i) {
      float a = 6.2831853f * static_cast<float>(i) / 24.0f;
      float r = avgRadius * (0.85f + 0.15f * std::sin(a * 3.0f + tick_phase(w, group.id, 0.05f)));
      glVertex2f(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
    }
    glEnd();

    glColor4f(col[0], col[1], col[2], 0.22f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);
    for (int i = 0; i <= 24; ++i) {
      float a = 6.2831853f * static_cast<float>(i) / 24.0f;
      glVertex2f(center.x + std::cos(a) * avgRadius, center.y + std::sin(a) * avgRadius);
    }
    glEnd();

    glColor4f(col[0], col[1], col[2], 0.45f);
    glBegin(GL_LINES);
    for (const auto* u : units) {
      glVertex2f(center.x, center.y);
      glVertex2f(u->renderPos.x, u->renderPos.y);
    }
    glEnd();
    ++gStrategicCounters.armyFormationVisuals;
  }
  glLineWidth(1.0f);
  glDisable(GL_BLEND);
}

void draw_combat_encounter_markers(const dom::sim::World& w, const Camera& c) {
  if (c.zoom > 62.0f) return;
  struct EncounterMarker { glm::vec2 pos{}; uint16_t a{UINT16_MAX}; uint16_t b{UINT16_MAX}; uint32_t hash{0}; };
  std::vector<EncounterMarker> markers;
  constexpr float kEngageDist = 2.8f;
  for (size_t i = 0; i < w.units.size(); ++i) {
    const auto& a = w.units[i];
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, a, 0)) continue;
    for (size_t j = i + 1; j < w.units.size(); ++j) {
      const auto& b = w.units[j];
      if (a.team == b.team || !dom::sim::players_at_war(w, a.team, b.team)) continue;
      if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, b, 0)) continue;
      float d = glm::length(a.pos - b.pos);
      if (d > kEngageDist) continue;
      glm::vec2 mid = (a.renderPos + b.renderPos) * 0.5f;
      bool merged = false;
      for (auto& m : markers) {
        if (glm::length(m.pos - mid) < 1.4f) {
          m.pos = (m.pos + mid) * 0.5f;
          merged = true;
          break;
        }
      }
      if (!merged) {
        uint16_t lo = std::min(a.team, b.team);
        uint16_t hi = std::max(a.team, b.team);
        markers.push_back({mid, lo, hi, (static_cast<uint32_t>(lo) << 16u) ^ hi ^ static_cast<uint32_t>(markers.size() * 2654435761u)});
      }
    }
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto& m : markers) {
    auto cA = diplomatic_color(w, m.a);
    auto cB = diplomatic_color(w, m.b);
    auto col = mix_color(cA, cB, 0.5f);
    float phase = tick_phase(w, m.hash, 0.09f);
    float radius = 0.72f + std::sin(phase) * 0.12f;
    draw_pulse_ring(m.pos, radius, 0.08f, 0.09f, phase, col);

    glColor4f(1.0f, 0.38f, 0.2f, 0.78f);
    glBegin(GL_LINES);
    glVertex2f(m.pos.x - 0.35f, m.pos.y - 0.35f);
    glVertex2f(m.pos.x + 0.35f, m.pos.y + 0.35f);
    glVertex2f(m.pos.x - 0.35f, m.pos.y + 0.35f);
    glVertex2f(m.pos.x + 0.35f, m.pos.y - 0.35f);
    glEnd();
    ++gStrategicCounters.combatEncounterMarkers;
  }
  glDisable(GL_BLEND);
}

void draw_unit_movement_paths(const dom::sim::World& w, const Camera& c) {
  if (c.zoom > 28.0f) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto& u : w.units) {
    if (!u.hasMoveOrder) continue;
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
    ++gStrategicCounters.movementPathResolves;
    glm::vec2 start = u.renderPos;
    glm::vec2 end = u.target;
    glm::vec2 delta = end - start;
    float len = glm::length(delta);
    if (len < 0.1f) {
      ++gStrategicCounters.visualFallbackCount;
      continue;
    }

    auto glyph = unit_glyph(u);
    float width = 1.2f;
    std::array<float,3> col = diplomatic_color(w, u.team);
    bool dashed = false;
    bool curved = false;
    bool railFollow = false;
    if (glyph == UnitGlyph::Armor) width = 2.6f;
    else if (glyph == UnitGlyph::Naval) { width = 2.0f; curved = true; col = {0.44f, 0.85f, 1.0f}; }
    else if (glyph == UnitGlyph::Aircraft) { width = 1.4f; dashed = true; col = {0.95f, 0.95f, 0.95f}; }
    else if (glyph == UnitGlyph::Rail) { width = 2.0f; railFollow = true; col = {0.98f, 0.9f, 0.42f}; }

    float phase = std::sin(tick_phase(w, u.id, 0.12f)) * 0.2f + 0.2f;
    glColor4f(col[0], col[1], col[2], 0.45f + phase);
    glLineWidth(width);

    if (railFollow && u.transportId != 0) {
      const dom::sim::Train* train = nullptr;
      for (const auto& t : w.trains) if (t.id == u.transportId) { train = &t; break; }
      if (train && !train->route.empty()) {
        bool foundNode = false;
        glm::vec2 rp = rail_node_pos(w, train->currentNode, foundNode);
        if (foundNode) {
          glBegin(GL_LINES);
          glVertex2f(start.x, start.y);
          glVertex2f(rp.x, rp.y);
          glEnd();
          draw_arrow_head(start, rp, 0.38f);
        } else {
          ++gStrategicCounters.visualFallbackCount;
        }
      }
    } else if (curved) {
      glm::vec2 mid = (start + end) * 0.5f;
      glm::vec2 perp{-delta.y, delta.x};
      float plen = glm::length(perp);
      if (plen > 0.0001f) perp = (perp / plen) * (std::min(4.0f, len * 0.25f));
      mid += perp;
      glBegin(GL_LINE_STRIP);
      for (int i = 0; i <= 12; ++i) {
        float t = i / 12.0f;
        glm::vec2 p = (1.0f - t) * (1.0f - t) * start + 2.0f * (1.0f - t) * t * mid + t * t * end;
        glVertex2f(p.x, p.y);
      }
      glEnd();
      draw_arrow_head(mid, end, 0.42f);
    } else if (dashed) {
      glm::vec2 dir = delta / len;
      float cursor = std::fmod(static_cast<float>((w.tick + u.id) % 20u) / 20.0f * 1.2f, 1.2f);
      while (cursor < len) {
        float a = cursor;
        float b = std::min(len, cursor + 0.8f);
        glm::vec2 p0 = start + dir * a;
        glm::vec2 p1 = start + dir * b;
        glBegin(GL_LINES);
        glVertex2f(p0.x, p0.y);
        glVertex2f(p1.x, p1.y);
        glEnd();
        cursor += 1.35f;
      }
      draw_arrow_head(start, end, 0.34f);
    } else {
      glBegin(GL_LINES);
      glVertex2f(start.x, start.y);
      glVertex2f(end.x, end.y);
      glEnd();
      draw_arrow_head(start, end, glyph == UnitGlyph::Armor ? 0.52f : 0.36f);
    }
  }

  for (const auto& u : w.units) {
    if (!u.hasMoveOrder) continue;
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
    float markerScale = c.zoom > 18.0f ? 0.20f : (c.zoom > 10.0f ? 0.24f : 0.30f);
    draw_unit_destination_marker(w, u, markerScale);
    ++gStrategicCounters.movementDestinationMarkers;
  }

  glLineWidth(1.0f);
  glDisable(GL_BLEND);
}

void draw_supply_and_logistics_flows(const dom::sim::World& w, const Camera& c) {
  if (c.zoom > 42.0f) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glLineWidth(1.1f);
  for (const auto& b : w.buildings) {
    bool source = b.type == dom::sim::BuildingType::FactoryHub || b.type == dom::sim::BuildingType::SteelMill || b.type == dom::sim::BuildingType::Refinery || b.type == dom::sim::BuildingType::Port;
    if (!source) continue;
    const dom::sim::Unit* best = nullptr;
    float bestDist = 99999.0f;
    for (const auto& u : w.units) {
      if (u.team != b.team) continue;
      float d = glm::length(u.pos - b.pos);
      if (d < bestDist) { bestDist = d; best = &u; }
    }
    if (!best) { ++gStrategicCounters.visualFallbackCount; continue; }
    ++gStrategicCounters.supplyFlowResolves;
    ++gStrategicCounters.logisticsVisualEvents;
    float pulse = 0.2f + (std::sin(tick_phase(w, b.id, 0.14f)) * 0.5f + 0.5f) * 0.3f;
    glColor4f(0.86f, 0.95f, 0.52f, pulse);
    glBegin(GL_LINES);
    glVertex2f(b.pos.x, b.pos.y);
    glVertex2f(best->renderPos.x, best->renderPos.y);
    glEnd();

    glm::vec2 d = best->renderPos - b.pos;
    float len = glm::length(d);
    if (len > 0.001f) {
      glm::vec2 p = b.pos + d * std::fmod(static_cast<float>((w.tick + b.id) % 100u) / 100.0f, 1.0f);
      glPointSize(2.5f);
      glBegin(GL_POINTS);
      glVertex2f(p.x, p.y);
      glEnd();
    }
  }

  for (const auto& tr : w.tradeRoutes) {
    if (!tr.active) continue;
    const dom::sim::City* a = nullptr;
    const dom::sim::City* b = nullptr;
    for (const auto& cty : w.cities) {
      if (cty.id == tr.fromCity) a = &cty;
      if (cty.id == tr.toCity) b = &cty;
    }
    if (!a || !b) { ++gStrategicCounters.visualFallbackCount; continue; }
    ++gStrategicCounters.supplyFlowResolves;
    glColor4f(0.52f, 0.9f, 0.95f, 0.26f);
    glBegin(GL_LINES);
    glVertex2f(a->pos.x, a->pos.y);
    glVertex2f(b->pos.x, b->pos.y);
    glEnd();
  }

  glDisable(GL_BLEND);
}

void draw_rail_traffic_visualization(const dom::sim::World& w, const Camera& c) {
  (void)c;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto& e : w.railEdges) {
    if (e.disrupted) continue;
    bool af = false;
    glm::vec2 a = rail_node_pos(w, e.aNode, af);
    bool bf = false;
    glm::vec2 b = rail_node_pos(w, e.bNode, bf);
    if (!af || !bf) { ++gStrategicCounters.visualFallbackCount; continue; }
    float phase = std::sin(tick_phase(w, e.id, 0.09f));
    glColor4f(0.95f, 0.88f, 0.38f, 0.12f + (phase * 0.5f + 0.5f) * 0.18f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(a.x, a.y); glVertex2f(b.x, b.y);
    glEnd();
    ++gStrategicCounters.railFlowLines;
  }

  glPointSize(c.zoom > 28.0f ? 2.5f : 3.5f);
  glBegin(GL_POINTS);
  for (const auto& t : w.trains) {
    if (t.state == dom::sim::TrainState::Inactive) continue;
    bool found = false;
    glm::vec2 p = rail_node_pos(w, t.currentNode, found);
    if (!found) { ++gStrategicCounters.visualFallbackCount; continue; }
    glColor4f(t.type == dom::sim::TrainType::Supply ? 0.45f : 0.95f, t.type == dom::sim::TrainType::Supply ? 0.95f : 0.78f, 0.35f, 0.95f);
    glVertex2f(p.x, p.y);
    ++gStrategicCounters.trainMarkers;
    ++gStrategicCounters.railVisualEvents;
  }
  glEnd();

  glPointSize(6.0f);
  glBegin(GL_POINTS);
  for (const auto& n : w.railNodes) {
    if (n.type != dom::sim::RailNodeType::Depot && n.type != dom::sim::RailNodeType::Station) continue;
    float pulse = 0.2f + (std::sin(tick_phase(w, n.id, 0.08f)) * 0.5f + 0.5f) * 0.25f;
    glColor4f(1.0f, 0.92f, 0.55f, pulse);
    glVertex2f(n.tile.x + 0.5f, n.tile.y + 0.5f);
    ++gStrategicCounters.railVisualEvents;
  }
  glEnd();
  glLineWidth(1.0f);
  glDisable(GL_BLEND);
}

void draw_frontline_pressure_and_theater(const dom::sim::World& w, const Camera& c) {
  (void)c;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (int y = 1; y < w.height - 1; y += 2) {
    for (int x = 1; x < w.width - 1; x += 2) {
      int i = y * w.width + x;
      uint16_t owner = w.territoryOwner[i];
      if (owner == UINT16_MAX || owner == 0) continue;
      bool frontline = tile_frontline(w, x, y, owner);
      if (!frontline) continue;
      ++gStrategicCounters.frontlineZoneUpdates;
      float pulse = 0.08f + (std::sin(tick_phase(w, static_cast<uint32_t>(i), 0.07f)) * 0.5f + 0.5f) * 0.12f;
      glColor4f(1.0f, 0.24f, 0.2f, pulse);
      glBegin(GL_QUADS);
      glVertex2f((float)x, (float)y);
      glVertex2f((float)x + 1.0f, (float)y);
      glVertex2f((float)x + 1.0f, (float)y + 1.0f);
      glVertex2f((float)x, (float)y + 1.0f);
      glEnd();
    }
  }

  glLineWidth(1.4f);
  for (const auto& o : w.operationalObjectives) {
    if (!o.active) continue;
    ++gStrategicCounters.theaterVisualResolves;
    glm::vec2 target{(o.targetRegion.x + o.targetRegion.z) * 0.5f, (o.targetRegion.y + o.targetRegion.w) * 0.5f};
    glm::vec2 origin = target;
    for (const auto& th : w.theaterCommands) {
      if (th.theaterId != o.theaterId) continue;
      origin = {(th.bounds.x + th.bounds.z) * 0.5f, (th.bounds.y + th.bounds.w) * 0.5f};
      break;
    }
    auto tcol = diplomatic_color(w, o.owner);
    glColor4f(tcol[0], tcol[1], tcol[2], 0.55f);
    glBegin(GL_LINES);
    glVertex2f(origin.x, origin.y);
    glVertex2f(target.x, target.y);
    glEnd();
    draw_arrow_head(origin, target, 0.55f);
    draw_ring(target, 0.85f, 0.07f, {std::min(1.0f, tcol[0] + 0.25f), std::min(1.0f, tcol[1] + 0.25f), std::min(1.0f, tcol[2] + 0.25f)});
  }

  for (const auto& th : w.theaterCommands) {
    glm::vec2 p{(th.bounds.x + th.bounds.z) * 0.5f, (th.bounds.y + th.bounds.w) * 0.5f};
    float heat = std::clamp(th.threatLevel, 0.0f, 1.0f);
    if (heat <= 0.01f) continue;
    ++gStrategicCounters.frontlineZoneUpdates;
    draw_pulse_ring(p, 1.6f + heat, 0.35f, 0.12f, tick_phase(w, th.theaterId, 0.06f), {1.0f, 0.45f + (1.0f-heat)*0.2f, 0.3f});
  }

  glLineWidth(1.0f);
  glDisable(GL_BLEND);
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


void draw_territory_readability_overlay(const dom::sim::World& w, const Camera& c) {
  const int step = c.zoom > 65.0f ? 3 : (c.zoom > 34.0f ? 2 : 1);
  const float alpha = c.zoom > 70.0f ? 0.28f : (c.zoom > 36.0f ? 0.24f : 0.18f);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  for (int y = 0; y < w.height - step; y += step) {
    for (int x = 0; x < w.width - step; x += step) {
      size_t i = static_cast<size_t>(y * w.width + x);
      uint16_t owner = w.territoryOwner[i];
      if (owner == UINT16_MAX) continue;
      auto col = owner_tint_color(w, owner);
      glColor4f(col[0], col[1], col[2], alpha);
      glVertex2f((float)x, (float)y);
      glVertex2f((float)(x + step), (float)y);
      glVertex2f((float)(x + step), (float)(y + step));
      glVertex2f((float)x, (float)(y + step));
    }
  }
  glEnd();
  glDisable(GL_BLEND);
}

void draw_colored_borders(const dom::sim::World& w, const Camera& c) {
  glLineWidth(c.zoom > 55.0f ? 1.0f : 1.8f);
  glBegin(GL_LINES);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      int i = y * w.width + x;
      uint16_t owner = w.territoryOwner[i];
      if (owner == UINT16_MAX) continue;
      auto col = owner_tint_color(w, owner);
      float glow = c.zoom > 60.0f ? 0.84f : 1.0f;
      glColor3f(std::min(1.0f, col[0] * glow + 0.15f), std::min(1.0f, col[1] * glow + 0.15f), std::min(1.0f, col[2] * glow + 0.15f));
      if (x + 1 < w.width && w.territoryOwner[i + 1] != owner) {
        glVertex2f(x + 1.0f, y + 0.02f); glVertex2f(x + 1.0f, y + 0.98f);
      }
      if (y + 1 < w.height && w.territoryOwner[i + w.width] != owner) {
        glVertex2f(x + 0.02f, y + 1.0f); glVertex2f(x + 0.98f, y + 1.0f);
      }
    }
  }
  glEnd();
}

void draw_strategic_region_overlays(const dom::sim::World& w, const Camera& c) {
  (void)c;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  for (int y = 1; y < w.height - 1; ++y) {
    for (int x = 1; x < w.width - 1; ++x) {
      int i = y * w.width + x;
      uint16_t owner = w.territoryOwner[i];
      if (owner == UINT16_MAX) continue;
      bool contested = (w.territoryOwner[i - 1] != owner) || (w.territoryOwner[i + 1] != owner) || (w.territoryOwner[i - w.width] != owner) || (w.territoryOwner[i + w.width] != owner);
      bool frontline = tile_frontline(w, x, y, owner);
      bool recent = tile_recently_captured(w, x, y, 900u);
      if (!contested && !frontline && !recent && !w.armageddonActive && w.activeWorldEventCount == 0) continue;
      std::array<float,3> col{0.95f, 0.90f, 0.35f};
      float alpha = 0.0f;
      if (contested) { col = {0.96f, 0.78f, 0.26f}; alpha = std::max(alpha, 0.18f); }
      if (frontline) { col = {1.0f, 0.30f, 0.24f}; alpha = std::max(alpha, 0.23f); }
      if (recent) { col = {0.98f, 0.94f, 0.55f}; alpha = std::max(alpha, 0.20f); }
      if (w.activeWorldEventCount > 0) { col = {0.88f, 0.34f, 0.9f}; alpha = std::max(alpha, 0.12f); }
      if (w.armageddonActive) { col = {0.95f, 0.16f, 0.22f}; alpha = std::max(alpha, 0.16f); }
      glColor4f(col[0], col[1], col[2], alpha);
      glVertex2f((float)x, (float)y);
      glVertex2f((float)x + 1.0f, (float)y);
      glVertex2f((float)x + 1.0f, (float)y + 1.0f);
      glVertex2f((float)x, (float)y + 1.0f);
    }
  }
  glEnd();
  glDisable(GL_BLEND);
}

void draw_strategic_labels(const dom::sim::World& w, const Camera& c) {
  if (c.zoom < 18.0f) return;
  glPointSize(c.zoom > 60.0f ? 4.0f : 6.0f);
  glBegin(GL_POINTS);
  for (const auto& cty : w.cities) {
    if (!cty.capital && cty.level < 4) continue;
    auto col = cty.capital ? std::array<float,3>{1.0f, 0.95f, 0.35f} : std::array<float,3>{0.86f, 0.86f, 0.9f};
    glColor3f(col[0], col[1], col[2]);
    glVertex2f(cty.pos.x, cty.pos.y);
  }
  for (const auto& th : w.theaterCommands) {
    glm::vec2 p{(th.bounds.x + th.bounds.z) * 0.5f, (th.bounds.y + th.bounds.w) * 0.5f};
    glColor3f(0.6f, 0.9f, 1.0f);
    glVertex2f(p.x, p.y);
  }
  for (const auto& s : w.guardianSites) {
    if (!s.discovered && !w.godMode) continue;
    glColor3f(0.9f, 0.45f, 0.95f);
    glVertex2f(s.pos.x, s.pos.y);
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
    std::string rc = "industrial_node";
    if (rn.type == dom::sim::ResourceNodeType::Forest) rc = "forest_site";
    else if (rn.type == dom::sim::ResourceNodeType::Ore) rc = "mine_entrance";
    else if (rn.type == dom::sim::ResourceNodeType::Farmable) rc = "settlement_object";
    else if (rn.type == dom::sim::ResourceNodeType::Ruins) rc = "capital_landmark";
    const auto objStyle = resolve_render_style({RenderStyleDomain::Object, {}, {}, {}, rc, {}, {}, strategic ? ContentLodTier::Far : ContentLodTier::Near});
    std::array<float, 3> col{objStyle.tint[0], objStyle.tint[1], objStyle.tint[2]};
    draw_feature_circle(rn.pos, (strategic ? 0.16f : 0.24f) * objStyle.sizeScale[0], col);
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
    const auto gStyle = resolve_render_style({RenderStyleDomain::Object, s.guardianId, {}, {}, "guardian_site", (s.spawned && s.alive) ? "strategic_warning" : "default", {}, strategic ? ContentLodTier::Far : ContentLodTier::Near});
    std::array<float, 3> col{gStyle.tint[0], gStyle.tint[1], gStyle.tint[2]};
    draw_feature_circle(s.pos, (strategic ? 0.26f : 0.34f) * gStyle.sizeScale[0], col);
  }
}

void draw_strategic_geography_grounding(const dom::sim::World& w, const Camera& c) {
  const float roadBaseWidth = c.zoom > 34.0f ? 1.0f : 2.4f;
  const float railBaseWidth = c.zoom > 34.0f ? 1.2f : 2.9f;
  const float riverWidth = c.zoom > 46.0f ? 1.0f : 2.1f;

  std::unordered_map<uint32_t, glm::vec2> railNodeCache;
  railNodeCache.reserve(w.railNodes.size());
  for (const auto& n : w.railNodes) railNodeCache[n.id] = {n.tile.x + 0.5f, n.tile.y + 0.5f};

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glLineWidth(roadBaseWidth + 2.0f);
  glBegin(GL_LINES);
  for (const auto& r : w.roads) {
    glColor4f(0.12f, 0.10f, 0.08f, 0.35f);
    glVertex2f(static_cast<float>(r.a.x) + 0.5f, static_cast<float>(r.a.y) + 0.5f);
    glVertex2f(static_cast<float>(r.b.x) + 0.5f, static_cast<float>(r.b.y) + 0.5f);
  }
  glEnd();

  glLineWidth(roadBaseWidth);
  glBegin(GL_LINES);
  for (const auto& r : w.roads) {
    float quality = std::clamp(r.quality / 3.0f, 0.35f, 1.0f);
    std::array<float, 3> rc = r.owner == 0 ? std::array<float, 3>{0.94f, 0.86f, 0.52f}
                                            : (r.owner == 1 ? std::array<float, 3>{0.45f, 0.79f, 0.95f}
                                                            : std::array<float, 3>{0.72f, 0.72f, 0.72f});
    rc = mix_color(rc, {0.52f, 0.36f, 0.2f}, 0.35f);
    glColor4f(rc[0], rc[1], rc[2], 0.52f + quality * 0.34f);
    glVertex2f(static_cast<float>(r.a.x) + 0.5f, static_cast<float>(r.a.y) + 0.5f);
    glVertex2f(static_cast<float>(r.b.x) + 0.5f, static_cast<float>(r.b.y) + 0.5f);
  }
  glEnd();

  glLineWidth(railBaseWidth + 2.2f);
  glBegin(GL_LINES);
  for (const auto& e : w.railEdges) {
    auto ai = railNodeCache.find(e.aNode);
    auto bi = railNodeCache.find(e.bNode);
    if (ai == railNodeCache.end() || bi == railNodeCache.end()) continue;
    glColor4f(0.08f, 0.08f, 0.1f, 0.5f);
    glVertex2f(ai->second.x, ai->second.y);
    glVertex2f(bi->second.x, bi->second.y);
  }
  glEnd();

  glLineWidth(railBaseWidth);
  glBegin(GL_LINES);
  for (const auto& e : w.railEdges) {
    auto ai = railNodeCache.find(e.aNode);
    auto bi = railNodeCache.find(e.bNode);
    if (ai == railNodeCache.end() || bi == railNodeCache.end()) continue;
    std::array<float, 3> col = e.disrupted ? std::array<float, 3>{0.92f, 0.28f, 0.22f} : std::array<float, 3>{0.86f, 0.84f, 0.78f};
    if (e.bridge) col = mix_color(col, {0.98f, 0.9f, 0.45f}, 0.55f);
    if (e.tunnel) col = mix_color(col, {0.56f, 0.56f, 0.62f}, 0.35f);
    float quality = std::clamp(e.quality / 3.0f, 0.3f, 1.0f);
    glColor4f(col[0], col[1], col[2], 0.65f + quality * 0.25f);
    glVertex2f(ai->second.x, ai->second.y);
    glVertex2f(bi->second.x, bi->second.y);
  }
  glEnd();

  glLineWidth(std::max(1.0f, riverWidth + 2.0f));
  glBegin(GL_LINES);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      size_t i = static_cast<size_t>(y * w.width + x);
      if (w.riverMap.empty() || i >= w.riverMap.size() || w.riverMap[i] == 0) continue;
      glm::vec2 p{x + 0.5f, y + 0.5f};
      const int ox[4] = {1, 0, -1, 0};
      const int oy[4] = {0, 1, 0, -1};
      for (int d = 0; d < 4; ++d) {
        int nx = x + ox[d], ny = y + oy[d];
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        size_t ni = static_cast<size_t>(ny * w.width + nx);
        if (ni >= w.riverMap.size() || w.riverMap[ni] == 0) continue;
        if (nx < x || (nx == x && ny < y)) continue;
        glColor4f(0.08f, 0.16f, 0.22f, 0.38f);
        glVertex2f(p.x, p.y);
        glVertex2f(nx + 0.5f, ny + 0.5f);
      }
    }
  }
  glEnd();

  glLineWidth(riverWidth);
  glBegin(GL_LINES);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      size_t i = static_cast<size_t>(y * w.width + x);
      if (w.riverMap.empty() || i >= w.riverMap.size() || w.riverMap[i] == 0) continue;
      glm::vec2 p{x + 0.5f, y + 0.5f};
      const int ox[4] = {1, 0, -1, 0};
      const int oy[4] = {0, 1, 0, -1};
      for (int d = 0; d < 4; ++d) {
        int nx = x + ox[d], ny = y + oy[d];
        if (nx < 0 || ny < 0 || nx >= w.width || ny >= w.height) continue;
        size_t ni = static_cast<size_t>(ny * w.width + nx);
        if (ni >= w.riverMap.size() || w.riverMap[ni] == 0) continue;
        if (nx < x || (nx == x && ny < y)) continue;
        glColor4f(0.22f, 0.68f, 0.94f, 0.78f);
        glVertex2f(p.x, p.y);
        glVertex2f(nx + 0.5f, ny + 0.5f);
      }
    }
  }
  glEnd();

  glPointSize(c.zoom > 35.0f ? 3.0f : 4.0f);
  glBegin(GL_POINTS);
  for (const auto& e : w.railEdges) {
    if (!e.bridge) continue;
    auto ai = railNodeCache.find(e.aNode);
    auto bi = railNodeCache.find(e.bNode);
    if (ai == railNodeCache.end() || bi == railNodeCache.end()) continue;
    glm::vec2 mid = (ai->second + bi->second) * 0.5f;
    glColor4f(1.0f, 0.92f, 0.56f, e.disrupted ? 0.45f : 0.96f);
    glVertex2f(mid.x, mid.y);
  }
  for (const auto& r : w.roads) {
    glm::ivec2 c0 = r.a;
    glm::ivec2 c1 = r.b;
    const int dx = std::abs(c1.x - c0.x);
    const int dy = std::abs(c1.y - c0.y);
    const int steps = std::max(dx, dy);
    if (steps <= 0) continue;
    bool crossing = false;
    for (int s = 0; s <= steps; ++s) {
      float t = static_cast<float>(s) / static_cast<float>(steps);
      int sx = std::clamp(static_cast<int>(std::round(c0.x + (c1.x - c0.x) * t)), 0, w.width - 1);
      int sy = std::clamp(static_cast<int>(std::round(c0.y + (c1.y - c0.y) * t)), 0, w.height - 1);
      size_t si = static_cast<size_t>(sy * w.width + sx);
      if (!w.riverMap.empty() && si < w.riverMap.size() && w.riverMap[si] != 0) {
        crossing = true;
        break;
      }
    }
    if (!crossing) continue;
    glm::vec2 mid = {((float)c0.x + (float)c1.x) * 0.5f + 0.5f, ((float)c0.y + (float)c1.y) * 0.5f + 0.5f};
    glColor4f(0.98f, 0.92f, 0.66f, 0.9f);
    glVertex2f(mid.x, mid.y);
  }
  glEnd();

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

void plot_line(std::vector<uint8_t>& pix, int res, int x0, int y0, int x1, int y1, const std::array<uint8_t, 3>& rgb) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    plot_dot(pix, res, x0, y0, rgb, 0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
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
      auto sample = resolve_terrain_visual_blended(w, gx + 0.5f, gy + 0.5f);
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

  for (int y = 1; y < w.height - 1; ++y) {
    for (int x = 1; x < w.width - 1; ++x) {
      size_t i = static_cast<size_t>(y * w.width + x);
      if (w.riverMap.empty() || i >= w.riverMap.size() || w.riverMap[i] == 0) continue;
      int px = world_to_minimap_px(x + 0.5f, static_cast<float>(w.width), res);
      int py = world_to_minimap_px(y + 0.5f, static_cast<float>(w.height), res);
      plot_dot(out, res, px, py, {74, 170, 230}, 0);
    }
  }

  for (const auto& r : w.roads) {
    int ax = world_to_minimap_px(static_cast<float>(r.a.x) + 0.5f, static_cast<float>(w.width), res);
    int ay = world_to_minimap_px(static_cast<float>(r.a.y) + 0.5f, static_cast<float>(w.height), res);
    int bx = world_to_minimap_px(static_cast<float>(r.b.x) + 0.5f, static_cast<float>(w.width), res);
    int by = world_to_minimap_px(static_cast<float>(r.b.y) + 0.5f, static_cast<float>(w.height), res);
    plot_line(out, res, ax, ay, bx, by, {180, 154, 88});
  }

  std::unordered_map<uint32_t, glm::vec2> railNodeCache;
  railNodeCache.reserve(w.railNodes.size());
  for (const auto& n : w.railNodes) railNodeCache[n.id] = {n.tile.x + 0.5f, n.tile.y + 0.5f};
  for (const auto& e : w.railEdges) {
    auto ai = railNodeCache.find(e.aNode);
    auto bi = railNodeCache.find(e.bNode);
    if (ai == railNodeCache.end() || bi == railNodeCache.end()) continue;
    int ax = world_to_minimap_px(ai->second.x, static_cast<float>(w.width), res);
    int ay = world_to_minimap_px(ai->second.y, static_cast<float>(w.height), res);
    int bx = world_to_minimap_px(bi->second.x, static_cast<float>(w.width), res);
    int by = world_to_minimap_px(bi->second.y, static_cast<float>(w.height), res);
    plot_line(out, res, ax, ay, bx, by, e.disrupted ? std::array<uint8_t, 3>{212, 78, 70} : std::array<uint8_t, 3>{220, 214, 168});
  }

  for (const auto& c : w.cities) {
    int gx = std::clamp(static_cast<int>(c.pos.x), 0, w.width - 1);
    int gy = std::clamp(static_cast<int>(c.pos.y), 0, w.height - 1);
    if (!w.godMode && w.fog[gy * w.width + gx] > 0 && c.team != 0) continue;
    int px = world_to_minimap_px(c.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(c.pos.y, static_cast<float>(w.height), res);
    auto rgb = team_rgb(c.team);
    int dot = c.capital ? 3 : (c.level >= 4 ? 2 : 1);
    plot_dot(out, res, px, py, rgb, dot);
    if (c.capital) {
      plot_dot(out, res, px, py, {255, 245, 150}, 1);
    }
  }

  for (const auto& b : w.buildings) {
    int px = world_to_minimap_px(b.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(b.pos.y, static_cast<float>(w.height), res);
    if (b.type == dom::sim::BuildingType::Port) plot_dot(out, res, px, py, {80, 188, 240}, 0);
    if (b.type == dom::sim::BuildingType::Mine) plot_dot(out, res, px, py, {224, 194, 78}, 0);
    if (b.type == dom::sim::BuildingType::FactoryHub || b.type == dom::sim::BuildingType::SteelMill || b.type == dom::sim::BuildingType::Refinery) {
      plot_dot(out, res, px, py, {206, 132, 78}, 0);
    }
  }

  for (const auto& n : w.railNodes) {
    int px = world_to_minimap_px(n.tile.x + 0.5f, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(n.tile.y + 0.5f, static_cast<float>(w.height), res);
    if (n.type == dom::sim::RailNodeType::Depot || n.type == dom::sim::RailNodeType::Station) {
      plot_dot(out, res, px, py, {224, 224, 120}, 0);
    }
  }

  for (int y = 1; y < res - 1; ++y) {
    for (int x = 1; x < res - 1; ++x) {
      int gx = std::clamp(static_cast<int>((static_cast<float>(x) / res) * w.width), 0, w.width - 2);
      int gy = std::clamp(static_cast<int>((static_cast<float>(y) / res) * w.height), 0, w.height - 2);
      size_t gi = static_cast<size_t>(gy * w.width + gx);
      uint16_t owner = w.territoryOwner[gi];
      if (owner == UINT16_MAX) continue;
      if (w.territoryOwner[gi + 1] != owner || w.territoryOwner[gi + w.width] != owner) {
        auto col = team_rgb(owner > 0 ? static_cast<uint16_t>(owner - 1) : 0u);
        plot_dot(out, res, x, y, col, 0);
      }
    }
  }

  for (const auto& u : w.units) {
    if (!w.godMode && !dom::sim::is_unit_visible_to_player(w, u, 0)) continue;
    int px = world_to_minimap_px(u.pos.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(u.pos.y, static_cast<float>(w.height), res);
    plot_dot(out, res, px, py, team_rgb(u.team), 0);
  }

  for (const auto& th : w.theaterCommands) {
    glm::vec2 p{(th.bounds.x + th.bounds.z) * 0.5f, (th.bounds.y + th.bounds.w) * 0.5f};
    int px = world_to_minimap_px(p.x, static_cast<float>(w.width), res);
    int py = world_to_minimap_px(p.y, static_cast<float>(w.height), res);
    plot_dot(out, res, px, py, {120, 220, 255}, 1);
  }

  if (w.activeWorldEventCount > 0 || w.armageddonActive) {
    for (const auto& c : w.cities) {
      int px = world_to_minimap_px(c.pos.x, static_cast<float>(w.width), res);
      int py = world_to_minimap_px(c.pos.y, static_cast<float>(w.height), res);
      plot_dot(out, res, px, py, w.armageddonActive ? std::array<uint8_t,3>{255, 60, 70} : std::array<uint8_t,3>{220, 120, 255}, 0);
    }
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
  load_render_stylesheets();
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
  reset_content_resolution_counters();
  reset_render_stylesheet_counters();
  gEntityCounters = {};
  gStrategicCounters = {};
  std::vector<TerrainChunkMesh> terrainChunks;
  build_terrain_chunk_meshes(w, 16, terrainChunks);
  glBegin(GL_TRIANGLES);
  for (const auto& chunk : terrainChunks) {
    for (const auto& v : chunk.triangles) {
      glColor3f(v.color.r, v.color.g, v.color.b);
      glVertex2f(v.x, v.y);
    }
  }
  glEnd();

  glBegin(GL_LINES);
  for (int y = 1; y < w.height - 1; ++y) {
    for (int x = 1; x < w.width - 1; ++x) {
      size_t i = y * w.width + x;
      auto sample = resolve_terrain_visual_blended(w, x + 0.5f, y + 0.5f);
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
        auto sample = resolve_terrain_visual_blended(w, x + 0.5f, y + 0.5f);
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
  if (gOverlay.showTerritory) {
    draw_textured_overlay(gOverlay.territoryTex, w, 0.08f, {0.62f, 0.62f, 0.74f}, true);
    draw_territory_readability_overlay(w, c);
  }
  if (gOverlay.showBorders) {
    draw_textured_overlay(gOverlay.borderTex, w, 0.15f, {1.0f, 1.0f, 0.95f}, true);
    draw_colored_borders(w, c);
  }
  if (gOverlay.showStrategicOverlays) {
    draw_strategic_region_overlays(w, c);
    if (gOverlay.showLabels) draw_strategic_labels(w, c);
  }
  if (gOverlay.showStrategicOverlays) {
    draw_frontline_pressure_and_theater(w, c);
    draw_supply_and_logistics_flows(w, c);
    draw_rail_traffic_visualization(w, c);
    draw_unit_movement_paths(w, c);
    draw_army_group_formations(w, c);
    draw_combat_encounter_markers(w, c);
  }
  if (gOverlay.showFog && !w.godMode) draw_textured_overlay(gOverlay.fogTex, w, 1.0f, {0.0f, 0.0f, 0.0f}, true);

  draw_forest_and_feature_markers(w, c);
  draw_strategic_geography_grounding(w, c);

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
    if (bPres.iconId.find("fallback") != std::string::npos) { ++gEntityCounters.entityPresentationFallbacks; }
    auto rel = diplomatic_color(w, b.team);
    auto theme = theme_tint_for_team(w, b.team);
    auto base = mix_color(rel, theme, 0.35f);
    if (b.underConstruction) base = mix_color(base, {0.35f, 0.35f, 0.35f}, 0.45f);
    if (b.factory.blocked) base = {0.88f, 0.35f, 0.25f};
    else if (b.factory.active) base = mix_color(base, {0.95f, 0.95f, 0.42f}, 0.2f);
    const bool logistics = b.type == dom::sim::BuildingType::Port || b.type == dom::sim::BuildingType::Market;
    const bool mythic = b.type == dom::sim::BuildingType::Wonder;
    std::string state = b.underConstruction ? "construction" : (b.hp < b.maxHp * 0.5f ? "damaged" : "default");
    if (b.factory.blocked) state = "strategic_warning";
    const auto bStyle = resolve_render_style({RenderStyleDomain::Building, b.definitionId, normalized_civ_key(w, b.team), team_theme_id(w, b.team), building_render_class(b.type), state, {}, select_lod_tier(c.zoom)});
    if (bStyle.fallback) ++gEntityCounters.entityPresentationFallbacks;
    base = mix_color(base, {bStyle.tint[0], bStyle.tint[1], bStyle.tint[2]}, 0.25f);
    float sx = b.size.x * 0.5f * bStyle.sizeScale[0];
    float sy = b.size.y * 0.5f * bStyle.sizeScale[1];
    glColor4f(0.06f, 0.05f, 0.04f, 0.35f);
    glVertex2f(b.pos.x - sx * 0.95f, b.pos.y - sy * 0.95f + 0.1f);
    glVertex2f(b.pos.x + sx * 0.95f, b.pos.y - sy * 0.95f + 0.1f);
    glVertex2f(b.pos.x + sx * 0.95f, b.pos.y + sy * 0.95f + 0.1f);
    glVertex2f(b.pos.x - sx * 0.95f, b.pos.y + sy * 0.95f + 0.1f);
    glColor3f(base[0], base[1], base[2]);
    glVertex2f(b.pos.x - sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y - sy);
    glVertex2f(b.pos.x + sx, b.pos.y + sy);
    glVertex2f(b.pos.x - sx, b.pos.y + sy);

    auto accent = mix_color(rel, {1.0f, 1.0f, 1.0f}, 0.2f);
    if (b.type == dom::sim::BuildingType::MissileSilo) accent = {0.95f, 0.24f, 0.24f};
    if (b.type == dom::sim::BuildingType::RadarTower || b.type == dom::sim::BuildingType::MobileRadar) accent = {0.35f, 0.95f, 0.95f};
    if (mythic) accent = {0.82f, 0.62f, 0.95f};
    if (logistics) accent = mix_color(accent, {0.92f, 0.86f, 0.44f}, 0.25f);
    glColor3f(accent[0], accent[1], accent[2]);
    float ax = sx * 0.42f;
    float ay = sy * 0.42f;
    glVertex2f(b.pos.x - ax, b.pos.y - ay);
    glVertex2f(b.pos.x + ax, b.pos.y - ay);
    glVertex2f(b.pos.x + ax, b.pos.y + ay);
    glVertex2f(b.pos.x - ax, b.pos.y + ay);

    if (b.type == dom::sim::BuildingType::Port || b.type == dom::sim::BuildingType::Mine || b.type == dom::sim::BuildingType::FactoryHub) {
      auto ring = b.type == dom::sim::BuildingType::Port ? std::array<float, 3>{0.44f, 0.82f, 0.94f}
                                                         : (b.type == dom::sim::BuildingType::Mine ? std::array<float, 3>{0.96f, 0.84f, 0.46f}
                                                                                                    : std::array<float, 3>{0.94f, 0.64f, 0.42f});
      glColor4f(ring[0], ring[1], ring[2], 0.28f);
      glVertex2f(b.pos.x - sx * 1.2f, b.pos.y - sy * 1.2f);
      glVertex2f(b.pos.x + sx * 1.2f, b.pos.y - sy * 1.2f);
      glVertex2f(b.pos.x + sx * 1.2f, b.pos.y + sy * 1.2f);
      glVertex2f(b.pos.x - sx * 1.2f, b.pos.y + sy * 1.2f);
    }
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
  const ContentLodTier lodTier = select_lod_tier(c.zoom);

  if (lodTier == ContentLodTier::Far && c.zoom >= clusterThreshold) {
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
      if (uPres.iconId.find("fallback") != std::string::npos) { ++gEntityCounters.entityPresentationFallbacks; }
      auto rel = unit_color(u);
      auto theme = theme_tint_for_team(w, u.team);
      auto base = mix_color(rel, theme, 0.25f);
      std::string uState = u.selected ? "selected" : (u.supplyState == dom::sim::SupplyState::LowSupply ? "low_supply" : "default");
      const auto uStyle = resolve_render_style({RenderStyleDomain::Unit, u.definitionId, normalized_civ_key(w, u.team), team_theme_id(w, u.team), unit_render_class(u), uState, {}, lodTier});
      if (uStyle.fallback) ++gEntityCounters.entityPresentationFallbacks;
      base = mix_color(base, {uStyle.tint[0], uStyle.tint[1], uStyle.tint[2]}, 0.2f);
      UnitGlyph glyph = unit_glyph(u);
      float s = (c.zoom < nearThreshold ? 0.38f : (c.zoom < farThreshold ? 0.46f : 0.50f)) * uStyle.sizeScale[0];
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
          if (glyph == UnitGlyph::RangedInfantry) rr *= (i % 2 == 0 ? 1.12f : 0.74f);
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

  struct TeamRegionMarkers { int industrial{0}; int ports{0}; int mines{0}; int rail{0}; };
  std::vector<TeamRegionMarkers> regionMarkers(w.players.size());
  for (const auto& b : w.buildings) {
    if (b.team >= regionMarkers.size()) continue;
    if (b.type == dom::sim::BuildingType::Port) ++regionMarkers[b.team].ports;
    if (b.type == dom::sim::BuildingType::Mine) ++regionMarkers[b.team].mines;
    if (b.type == dom::sim::BuildingType::FactoryHub || b.type == dom::sim::BuildingType::SteelMill || b.type == dom::sim::BuildingType::Refinery || b.type == dom::sim::BuildingType::MachineWorks || b.type == dom::sim::BuildingType::MunitionsPlant || b.type == dom::sim::BuildingType::ElectronicsLab) ++regionMarkers[b.team].industrial;
  }
  for (const auto& n : w.railNodes) {
    if (n.owner >= regionMarkers.size()) continue;
    if (n.type == dom::sim::RailNodeType::Depot || n.type == dom::sim::RailNodeType::Station) ++regionMarkers[n.owner].rail;
  }

  for (const auto& cty : w.cities) {
    int gx = std::clamp(static_cast<int>(cty.pos.x), 0, w.width - 1);
    int gy = std::clamp(static_cast<int>(cty.pos.y), 0, w.height - 1);
    if (!w.godMode && w.fog[gy * w.width + gx] > 0 && cty.team != 0) continue;
    ++gEntityCounters.cityPresentationResolves;
    note_content_resolution(ContentResolutionDomain::CityRegion, false);
    if (cty.capital) ++gEntityCounters.capitalPresentationResolves;

    TeamRegionMarkers markers{};
    if (cty.team < regionMarkers.size()) markers = regionMarkers[cty.team];

    auto rel = diplomatic_color(w, cty.team);
    auto theme = theme_tint_for_team(w, cty.team);
    auto col = mix_color(rel, theme, 0.3f);
    float core = cty.capital ? 1.28f : (cty.level >= 5 ? 1.16f : (cty.level >= 3 ? 1.0f : 0.76f));
    if (c.zoom > 80.0f) core *= 0.78f;
    else if (c.zoom < 20.0f) core *= 1.08f;

    CivSettlementShape shape = civ_settlement_shape(w, cty.team);

    glBegin(GL_QUADS);
    glColor3f(col[0], col[1], col[2]);
    glVertex2f(cty.pos.x - core, cty.pos.y - core * 0.85f);
    glVertex2f(cty.pos.x + core, cty.pos.y - core * 0.85f);
    glVertex2f(cty.pos.x + core, cty.pos.y + core * 0.85f);
    glVertex2f(cty.pos.x - core, cty.pos.y + core * 0.85f);
    glEnd();

    float kx = 0.42f;
    float ky = 0.58f;
    if (shape == CivSettlementShape::Rome) { kx = 0.54f; ky = 0.56f; }
    else if (shape == CivSettlementShape::China) { kx = 0.50f; ky = 0.48f; }
    else if (shape == CivSettlementShape::Japan) { kx = 0.38f; ky = 0.64f; }
    else if (shape == CivSettlementShape::Russia) { kx = 0.62f; ky = 0.46f; }
    else if (shape == CivSettlementShape::Tartaria) { kx = 0.34f; ky = 0.72f; }
    else if (shape == CivSettlementShape::Egypt || shape == CivSettlementShape::MiddleEast) { kx = 0.48f; ky = 0.64f; }
    else if (shape == CivSettlementShape::Eu || shape == CivSettlementShape::Uk || shape == CivSettlementShape::Usa) { kx = 0.56f; ky = 0.52f; }
    else { ++gEntityCounters.cityPresentationFallbacks; note_content_resolution(ContentResolutionDomain::CityRegion, true); }

    glBegin(GL_TRIANGLES);
    auto lm = cty.capital ? std::array<float,3>{1.0f, 0.95f, 0.65f} : std::array<float,3>{0.88f, 0.88f, 0.9f};
    glColor3f(lm[0], lm[1], lm[2]);
    float h = cty.capital ? 1.0f : (cty.level >= 4 ? 0.84f : 0.62f);
    glVertex2f(cty.pos.x, cty.pos.y + h * ky);
    glVertex2f(cty.pos.x - h * kx, cty.pos.y - h * 0.18f);
    glVertex2f(cty.pos.x + h * kx, cty.pos.y - h * 0.18f);
    glEnd();

    if (cty.level >= 5 && c.zoom < 70.0f) {
      glBegin(GL_QUADS);
      glColor3f(col[0] * 0.86f, col[1] * 0.86f, col[2] * 0.9f);
      glVertex2f(cty.pos.x - core * 1.5f, cty.pos.y - core * 0.35f);
      glVertex2f(cty.pos.x - core * 0.85f, cty.pos.y - core * 0.35f);
      glVertex2f(cty.pos.x - core * 0.85f, cty.pos.y + core * 0.35f);
      glVertex2f(cty.pos.x - core * 1.5f, cty.pos.y + core * 0.35f);
      glVertex2f(cty.pos.x + core * 0.85f, cty.pos.y - core * 0.35f);
      glVertex2f(cty.pos.x + core * 1.5f, cty.pos.y - core * 0.35f);
      glVertex2f(cty.pos.x + core * 1.5f, cty.pos.y + core * 0.35f);
      glVertex2f(cty.pos.x + core * 0.85f, cty.pos.y + core * 0.35f);
      glEnd();
    }

    if (cty.capital || cty.level >= 4) { (void)dom::ui::icons::resolve_marker_id("capital", cty.team); draw_ring(cty.pos, core + 0.45f, cty.capital ? 0.11f : 0.08f, cty.capital ? std::array<float,3>{0.98f, 0.92f, 0.36f} : std::array<float,3>{0.78f, 0.82f, 0.9f}); }

    auto regionMark = [&](float ox, float oy, const std::array<float, 3>& rc, float s) {
      glBegin(GL_QUADS);
      glColor3f(rc[0], rc[1], rc[2]);
      glVertex2f(cty.pos.x + ox - s, cty.pos.y + oy - s);
      glVertex2f(cty.pos.x + ox + s, cty.pos.y + oy - s);
      glVertex2f(cty.pos.x + ox + s, cty.pos.y + oy + s);
      glVertex2f(cty.pos.x + ox - s, cty.pos.y + oy + s);
      glEnd();
    };
    if (markers.industrial > 0) { ++gEntityCounters.regionPresentationResolves; ++gEntityCounters.industrialRegionMarkers; regionMark(core * 0.95f, -core * 0.7f, {0.88f, 0.52f, 0.34f}, 0.12f); }
    if (markers.ports > 0) { ++gEntityCounters.regionPresentationResolves; ++gEntityCounters.portRegionMarkers; regionMark(core * 0.2f, -core * 1.05f, {0.36f, 0.72f, 0.92f}, 0.11f); }
    if (markers.rail > 0) { ++gEntityCounters.regionPresentationResolves; ++gEntityCounters.railRegionMarkers; regionMark(-core * 0.95f, -core * 0.7f, {0.95f, 0.9f, 0.44f}, 0.1f); }
    if (markers.mines > 0) { ++gEntityCounters.regionPresentationResolves; ++gEntityCounters.miningRegionMarkers; regionMark(-core * 0.2f, -core * 1.05f, {0.94f, 0.84f, 0.42f}, 0.11f); }
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


void collect_strategic_label_hooks(const dom::sim::World& world, std::vector<StrategicLabelHook>& outHooks) {
  outHooks.clear();
  outHooks.reserve(world.cities.size() + world.theaterCommands.size() + world.guardianSites.size());
  for (const auto& c : world.cities) {
    if (!c.capital && c.level < 4) continue;
    StrategicLabelHook h{};
    h.pos = c.pos;
    h.owner = c.team;
    h.type = c.capital ? StrategicLabelType::Capital : StrategicLabelType::StrategicSite;
    h.text = c.capital ? "Capital" : "Major Region";
    outHooks.push_back(std::move(h));
  }
  for (const auto& t : world.theaterCommands) {
    StrategicLabelHook h{};
    h.owner = t.owner;
    h.type = StrategicLabelType::Theater;
    h.pos = {(t.bounds.x + t.bounds.z) * 0.5f, (t.bounds.y + t.bounds.w) * 0.5f};
    h.text = "Theater";
    outHooks.push_back(std::move(h));
  }
  for (const auto& s : world.guardianSites) {
    if (!s.discovered && !world.godMode) continue;
    StrategicLabelHook h{};
    h.owner = s.owner;
    h.type = StrategicLabelType::StrategicSite;
    h.pos = s.pos;
    h.text = "Strategic Site";
    outHooks.push_back(std::move(h));
  }
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
void set_strategic_visualization_enabled(bool enabled) { gOverlay.showStrategicOverlays = enabled; }
bool strategic_visualization_enabled() { return gOverlay.showStrategicOverlays; }
const VisualFeedbackCounters& visual_feedback_counters() { return gFeedbackCounters; }
const StrategicVisualizationCounters& strategic_visualization_counters() { return gStrategicCounters; }
double last_draw_ms() { return gLastDrawMs; }

void set_editor_preview(const EditorPreview& preview) { gEditorPreview = preview; }

} // namespace dom::render
