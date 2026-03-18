#pragma once

#include "engine/sim/simulation.h"
#include "engine/render/content_resolution.h"
#include <glm/vec3.hpp>
#include <cstdint>

namespace dom::render {

enum class TerrainMaterialId : uint8_t {
  Grassland,
  Steppe,
  ForestGround,
  Desert,
  Mediterranean,
  Jungle,
  Tundra,
  Snow,
  Wetlands,
  Mountain,
  SnowMountain,
  Littoral,
  River,
  Lake,
  ShallowOcean,
  DeepOcean,
};

struct TerrainVisualSample {
  TerrainMaterialId material{TerrainMaterialId::Grassland};
  glm::vec3 color{0.2f, 0.4f, 0.2f};
  glm::vec3 accent{0.2f, 0.4f, 0.2f};
  float ambient{0.5f};
  float directional{0.5f};
  float contrast{0.0f};
  float terrainBlend{0.12f};
  float slope{0.0f};
  float heightInfluence{0.0f};
  float waterEmphasis{0.0f};
  float macroVariation{0.0f};
  bool isWater{false};
  bool hasForestCanopy{false};
  bool hasCliff{false};
  bool mountain{false};
  bool snowCap{false};
};

struct TerrainPresentationCounters {
  uint64_t terrainMaterialResolves{0};
  uint64_t waterFeatureResolves{0};
  uint64_t forestClusterCount{0};
  uint64_t mountainFeatureCount{0};
  uint64_t terrainLightingSamples{0};
  uint64_t terrainContrastSamples{0};
  uint64_t terrainMaterialBlendSamples{0};
  uint64_t presentationFallbackCount{0};
};

TerrainVisualSample resolve_terrain_visual(const dom::sim::World& world, int cellIndex);
TerrainVisualSample resolve_terrain_visual(const dom::sim::World& world, int cellIndex, ContentLodTier lodTier);
TerrainVisualSample resolve_terrain_visual_blended(const dom::sim::World& world, float worldX, float worldY);
TerrainVisualSample resolve_terrain_visual_blended(const dom::sim::World& world, float worldX, float worldY, ContentLodTier lodTier);
float terrain_slope_hint(const dom::sim::World& world, int cellIndex);

void reset_terrain_presentation_counters();
void add_forest_cluster_counter(uint64_t count);
const TerrainPresentationCounters& terrain_presentation_counters();

} // namespace dom::render
