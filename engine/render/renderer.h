#pragma once
#include "engine/sim/simulation.h"
#include "engine/render/terrain_materials.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>

namespace dom::render {
struct Camera {
  glm::vec2 center{64, 64};
  float zoom{8.0f};
};

struct EntityPresentationCounters {
  uint64_t unitPresentationResolves{0};
  uint64_t buildingPresentationResolves{0};
  uint64_t cityPresentationResolves{0};
  uint64_t capitalPresentationResolves{0};
  uint64_t regionPresentationResolves{0};
  uint64_t industrialRegionMarkers{0};
  uint64_t portRegionMarkers{0};
  uint64_t railRegionMarkers{0};
  uint64_t miningRegionMarkers{0};
  uint64_t cityPresentationFallbacks{0};
  uint64_t guardianPresentationResolves{0};
  uint64_t entityPresentationFallbacks{0};
  uint64_t farLodClusterCount{0};
};

struct VisualFeedbackCounters {
  uint64_t combatEffectSpawns{0};
  uint64_t strategicEffectSpawns{0};
  uint64_t crisisEffectSpawns{0};
  uint64_t guardianEffectSpawns{0};
  uint64_t industryActivityEffects{0};
  uint64_t selectionFeedbackEvents{0};
  uint64_t feedbackFallbackCount{0};
};

struct StrategicVisualizationCounters {
  uint64_t movementPathResolves{0};
  uint64_t supplyFlowResolves{0};
  uint64_t railVisualEvents{0};
  uint64_t frontlineZoneUpdates{0};
  uint64_t theaterVisualResolves{0};
  uint64_t visualFallbackCount{0};
  uint64_t railFlowLines{0};
  uint64_t trainMarkers{0};
  uint64_t logisticsVisualEvents{0};
};

enum class StrategicLabelType : uint8_t {
  Capital,
  Theater,
  StrategicSite,
};

struct StrategicLabelHook {
  glm::vec2 pos{};
  StrategicLabelType type{StrategicLabelType::StrategicSite};
  uint16_t owner{UINT16_MAX};
  std::string text;
};

bool init_renderer();
void set_resolution(int width, int height);
void set_render_scale(float scale);
void set_ui_scale(float scale);
void draw(dom::sim::World& world, const Camera& camera, int width, int height, const std::vector<uint32_t>& dragHighlight);
uint32_t pick_unit(const dom::sim::World& world, const Camera& camera, int width, int height, glm::vec2 screen);
glm::vec2 screen_to_world(const Camera& camera, int width, int height, glm::vec2 screen);

void toggle_minimap();
bool minimap_screen_to_world(const dom::sim::World& world, int width, int height, glm::vec2 screen, glm::vec2& outWorld);
void generate_minimap_image(const dom::sim::World& world, int resolution, std::vector<uint8_t>& outRgb);
void collect_strategic_label_hooks(const dom::sim::World& world, std::vector<StrategicLabelHook>& outHooks);

void toggle_territory_overlay();
void toggle_border_overlay();
void toggle_fog_overlay();
void toggle_terrain_material_overlay();
void toggle_water_overlay();
void set_entity_presentation_debug(bool enabled);
bool entity_presentation_debug();
const EntityPresentationCounters& entity_presentation_counters();
void set_visual_feedback_enabled(bool enabled);
bool visual_feedback_enabled();
void set_visual_feedback_overlay_debug(bool enabled);
void set_strategic_visualization_enabled(bool enabled);
bool strategic_visualization_enabled();
bool visual_feedback_overlay_debug();
const VisualFeedbackCounters& visual_feedback_counters();
const StrategicVisualizationCounters& strategic_visualization_counters();
double last_draw_ms();

struct EditorPreview {
  bool enabled{false};
  glm::vec2 pos{};
  float radius{1.0f};
  float r{0.2f};
  float g{0.9f};
  float b{0.2f};
  float alpha{0.28f};
  bool valid{true};
};

void set_editor_preview(const EditorPreview& preview);
} // namespace dom::render
