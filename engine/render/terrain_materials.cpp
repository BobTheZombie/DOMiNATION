#include "engine/render/terrain_materials.h"
#include "engine/render/content_resolution.h"
#include "engine/render/render_stylesheet.h"
#include <algorithm>
#include <glm/common.hpp>

namespace dom::render {
namespace {
TerrainPresentationCounters gCounters{};

glm::vec3 material_color(TerrainMaterialId id) {
  switch (id) {
    case TerrainMaterialId::Grassland: return {0.39f, 0.58f, 0.30f};
    case TerrainMaterialId::Steppe: return {0.66f, 0.64f, 0.42f};
    case TerrainMaterialId::ForestGround: return {0.28f, 0.45f, 0.24f};
    case TerrainMaterialId::Desert: return {0.82f, 0.73f, 0.47f};
    case TerrainMaterialId::Mediterranean: return {0.70f, 0.64f, 0.42f};
    case TerrainMaterialId::Jungle: return {0.17f, 0.43f, 0.20f};
    case TerrainMaterialId::Tundra: return {0.60f, 0.66f, 0.56f};
    case TerrainMaterialId::Snow: return {0.89f, 0.92f, 0.96f};
    case TerrainMaterialId::Wetlands: return {0.33f, 0.46f, 0.35f};
    case TerrainMaterialId::Mountain: return {0.48f, 0.47f, 0.45f};
    case TerrainMaterialId::SnowMountain: return {0.86f, 0.88f, 0.91f};
    case TerrainMaterialId::Littoral: return {0.72f, 0.72f, 0.57f};
    case TerrainMaterialId::River: return {0.16f, 0.52f, 0.86f};
    case TerrainMaterialId::Lake: return {0.13f, 0.42f, 0.76f};
    case TerrainMaterialId::ShallowOcean: return {0.22f, 0.55f, 0.83f};
    case TerrainMaterialId::DeepOcean: return {0.07f, 0.20f, 0.50f};
  }
  ++gCounters.presentationFallbackCount;
  return {0.65f, 0.2f, 0.65f};
}




TerrainMaterialId material_from_set(std::string_view setId) {
  if (setId == "temperate_grass") return TerrainMaterialId::Grassland;
  if (setId == "steppe_dry") return TerrainMaterialId::Steppe;
  if (setId == "forest_floor") return TerrainMaterialId::ForestGround;
  if (setId == "sand_dune") return TerrainMaterialId::Desert;
  if (setId == "mediterranean_soil") return TerrainMaterialId::Mediterranean;
  if (setId == "jungle_floor") return TerrainMaterialId::Jungle;
  if (setId == "tundra_soil") return TerrainMaterialId::Tundra;
  if (setId == "snow") return TerrainMaterialId::Snow;
  if (setId == "marsh") return TerrainMaterialId::Wetlands;
  if (setId == "rock_highland") return TerrainMaterialId::Mountain;
  if (setId == "snow_rock_highland") return TerrainMaterialId::SnowMountain;
  if (setId == "coastal_mix") return TerrainMaterialId::Littoral;
  if (setId == "river_water") return TerrainMaterialId::River;
  if (setId == "lake_water") return TerrainMaterialId::Lake;
  if (setId == "shallow_water") return TerrainMaterialId::ShallowOcean;
  if (setId == "deep_water") return TerrainMaterialId::DeepOcean;
  ++gCounters.presentationFallbackCount;
  return TerrainMaterialId::Grassland;
}

float neighbor_water_factor(const dom::sim::World& world, int cellIndex) {
  int x = cellIndex % world.width;
  int y = cellIndex / world.width;
  int water = 0;
  int total = 0;
  constexpr int kDirs[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};
  for (const auto& d : kDirs) {
    int nx = x + d[0], ny = y + d[1];
    if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height) continue;
    ++total;
    int ni = ny * world.width + nx;
    auto tc = static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(ni)]);
    if (tc == dom::sim::TerrainClass::ShallowWater || tc == dom::sim::TerrainClass::DeepWater) ++water;
  }
  if (total == 0) return 0.0f;
  return static_cast<float>(water) / static_cast<float>(total);
}

bool is_land_coast(const dom::sim::World& world, int cellIndex) {
  if (cellIndex < 0 || cellIndex >= static_cast<int>(world.terrainClass.size())) return false;
  if (static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(cellIndex)]) != dom::sim::TerrainClass::Land) return false;
  int x = cellIndex % world.width;
  int y = cellIndex / world.width;
  constexpr int kDirs[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};
  for (const auto& d : kDirs) {
    int nx = x + d[0], ny = y + d[1];
    if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height) continue;
    int ni = ny * world.width + nx;
    auto tc = static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(ni)]);
    if (tc == dom::sim::TerrainClass::ShallowWater || tc == dom::sim::TerrainClass::DeepWater) return true;
  }
  return false;
}
} // namespace

float terrain_slope_hint(const dom::sim::World& world, int cellIndex) {
  if (cellIndex < 0 || cellIndex >= static_cast<int>(world.heightmap.size())) return 0.0f;
  int x = cellIndex % world.width;
  int y = cellIndex / world.width;
  float center = world.heightmap[static_cast<size_t>(cellIndex)];
  float maxDelta = 0.0f;
  constexpr int kDirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
  for (const auto& d : kDirs) {
    int nx = x + d[0], ny = y + d[1];
    if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height) continue;
    int ni = ny * world.width + nx;
    maxDelta = std::max(maxDelta, std::abs(center - world.heightmap[static_cast<size_t>(ni)]));
  }
  return std::clamp(maxDelta * 3.0f, 0.0f, 1.0f);
}

TerrainVisualSample resolve_terrain_visual(const dom::sim::World& world, int cellIndex) {
  TerrainVisualSample sample{};
  ++gCounters.terrainMaterialResolves;
  if (cellIndex < 0 || cellIndex >= static_cast<int>(world.heightmap.size())) {
    ++gCounters.presentationFallbackCount;
    return sample;
  }
  const auto tc = static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(cellIndex)]);
  const bool isRiver = !world.riverMap.empty() && world.riverMap[static_cast<size_t>(cellIndex)] > 0;
  const bool isLake = !world.lakeMap.empty() && world.lakeMap[static_cast<size_t>(cellIndex)] > 0;
  const float h = world.heightmap[static_cast<size_t>(cellIndex)];
  const float fertility = world.fertility[static_cast<size_t>(cellIndex)];
  const auto biome = dom::sim::biome_at(world, cellIndex);

  std::string renderClass = "grassland";
  std::string exactId;
  if (isRiver) { exactId = "river"; renderClass = "river_lake"; sample.isWater = true; ++gCounters.waterFeatureResolves; }
  else if (isLake) { exactId = "lake"; renderClass = "river_lake"; sample.isWater = true; ++gCounters.waterFeatureResolves; }
  else if (tc == dom::sim::TerrainClass::DeepWater) { renderClass = "deep_water"; sample.isWater = true; ++gCounters.waterFeatureResolves; }
  else if (tc == dom::sim::TerrainClass::ShallowWater) { renderClass = "shallow_water"; sample.isWater = true; ++gCounters.waterFeatureResolves; }
  else if (is_land_coast(world, cellIndex)) { renderClass = "coast_littoral"; }
  else {
    switch (biome) {
      case dom::sim::BiomeType::TemperateGrassland: renderClass = "grassland"; break;
      case dom::sim::BiomeType::Steppe: renderClass = "steppe"; break;
      case dom::sim::BiomeType::Forest: renderClass = "forest_ground"; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Desert: renderClass = "desert"; break;
      case dom::sim::BiomeType::Mediterranean: renderClass = "mediterranean"; break;
      case dom::sim::BiomeType::Jungle: renderClass = "jungle"; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Tundra: renderClass = "tundra"; break;
      case dom::sim::BiomeType::Arctic: renderClass = "snow_arctic"; break;
      case dom::sim::BiomeType::Coast: renderClass = "coast_littoral"; break;
      case dom::sim::BiomeType::Wetlands: renderClass = "wetlands_marsh"; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Mountain: renderClass = "mountains"; sample.mountain = true; ++gCounters.mountainFeatureCount; break;
      case dom::sim::BiomeType::SnowMountain: renderClass = "snow_mountains"; sample.mountain = true; sample.snowCap = true; ++gCounters.mountainFeatureCount; break;
      default: ++gCounters.presentationFallbackCount; renderClass = "grassland"; break;
    }
  }
  const auto style = resolve_render_style({RenderStyleDomain::Terrain, exactId, {}, {}, renderClass, {}, dom::sim::biome_runtime(biome).id, ContentLodTier::Near});
  if (style.fallback) ++gCounters.presentationFallbackCount;
  sample.material = material_from_set(style.materialSet);

  sample.color = material_color(sample.material);
  sample.accent = sample.color;

  if (!sample.isWater) {
    const float slope = terrain_slope_hint(world, cellIndex);
    const float waterAdj = neighbor_water_factor(world, cellIndex);
    const bool cliff = slope > 0.33f && h > 0.45f;
    sample.hasCliff = cliff;
    float shade = std::clamp(0.84f + h * 0.36f - slope * 0.2f + waterAdj * 0.05f, 0.62f, 1.26f);
    float fertMul = std::clamp(0.82f + fertility * 0.26f, 0.72f, 1.16f);
    sample.color *= shade * fertMul;
    if (sample.mountain) {
      sample.accent = glm::clamp(sample.color + glm::vec3(0.14f), glm::vec3(0.0f), glm::vec3(1.0f));
      if (sample.snowCap || h > 0.82f) {
        sample.snowCap = true;
        sample.accent = glm::vec3(0.92f, 0.94f, 0.97f);
      }
    }
    if (sample.material == TerrainMaterialId::Wetlands && isRiver) sample.color = glm::mix(sample.color, material_color(TerrainMaterialId::River), 0.35f);
    if (sample.material == TerrainMaterialId::Littoral) {
      sample.color = glm::mix(sample.color, material_color(TerrainMaterialId::ShallowOcean), 0.18f);
      sample.accent = glm::mix(sample.color, material_color(TerrainMaterialId::Desert), 0.1f);
    }
  } else if (sample.material == TerrainMaterialId::River || sample.material == TerrainMaterialId::Lake) {
    sample.accent = glm::mix(sample.color, material_color(TerrainMaterialId::ShallowOcean), 0.35f);
  }
  sample.color = glm::clamp(sample.color, glm::vec3(0.0f), glm::vec3(1.0f));
  sample.accent = glm::clamp(sample.accent, glm::vec3(0.0f), glm::vec3(1.0f));
  return sample;
}

void reset_terrain_presentation_counters() { gCounters = {}; }
void add_forest_cluster_counter(uint64_t count) { gCounters.forestClusterCount += count; }
const TerrainPresentationCounters& terrain_presentation_counters() { return gCounters; }

} // namespace dom::render
