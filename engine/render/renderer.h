#pragma once
#include "engine/sim/simulation.h"
#include "engine/render/terrain_materials.h"
#include <glm/vec2.hpp>
#include <vector>

namespace dom::render {
struct Camera {
  glm::vec2 center{64, 64};
  float zoom{8.0f};
};

struct EntityPresentationCounters {
  uint64_t unitPresentationResolves{0};
  uint64_t buildingPresentationResolves{0};
  uint64_t cityPresentationResolves{0};
  uint64_t guardianPresentationResolves{0};
  uint64_t entityPresentationFallbacks{0};
  uint64_t farLodClusterCount{0};
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

void toggle_territory_overlay();
void toggle_border_overlay();
void toggle_fog_overlay();
void toggle_terrain_material_overlay();
void toggle_water_overlay();
void set_entity_presentation_debug(bool enabled);
bool entity_presentation_debug();
const EntityPresentationCounters& entity_presentation_counters();
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
