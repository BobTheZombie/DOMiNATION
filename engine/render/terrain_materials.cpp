#include "engine/render/terrain_materials.h"
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

  if (isRiver) {
    sample.material = TerrainMaterialId::River;
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (isLake) {
    sample.material = TerrainMaterialId::Lake;
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (tc == dom::sim::TerrainClass::DeepWater) {
    sample.material = TerrainMaterialId::DeepOcean;
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (tc == dom::sim::TerrainClass::ShallowWater) {
    sample.material = TerrainMaterialId::ShallowOcean;
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (is_land_coast(world, cellIndex)) {
    sample.material = TerrainMaterialId::Littoral;
  } else {
    switch (biome) {
      case dom::sim::BiomeType::TemperateGrassland: sample.material = TerrainMaterialId::Grassland; break;
      case dom::sim::BiomeType::Steppe: sample.material = TerrainMaterialId::Steppe; break;
      case dom::sim::BiomeType::Forest: sample.material = TerrainMaterialId::ForestGround; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Desert: sample.material = TerrainMaterialId::Desert; break;
      case dom::sim::BiomeType::Mediterranean: sample.material = TerrainMaterialId::Mediterranean; break;
      case dom::sim::BiomeType::Jungle: sample.material = TerrainMaterialId::Jungle; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Tundra: sample.material = TerrainMaterialId::Tundra; break;
      case dom::sim::BiomeType::Arctic: sample.material = TerrainMaterialId::Snow; break;
      case dom::sim::BiomeType::Coast: sample.material = TerrainMaterialId::Littoral; break;
      case dom::sim::BiomeType::Wetlands: sample.material = TerrainMaterialId::Wetlands; sample.hasForestCanopy = true; break;
      case dom::sim::BiomeType::Mountain: sample.material = TerrainMaterialId::Mountain; sample.mountain = true; ++gCounters.mountainFeatureCount; break;
      case dom::sim::BiomeType::SnowMountain: sample.material = TerrainMaterialId::SnowMountain; sample.mountain = true; sample.snowCap = true; ++gCounters.mountainFeatureCount; break;
      default: ++gCounters.presentationFallbackCount; sample.material = TerrainMaterialId::Grassland; break;
    }
  }

  sample.color = material_color(sample.material);
  sample.accent = sample.color;

  if (!sample.isWater) {
    const float slope = terrain_slope_hint(world, cellIndex);
    const bool cliff = slope > 0.33f && h > 0.45f;
    sample.hasCliff = cliff;
    float shade = std::clamp(0.85f + h * 0.35f - slope * 0.18f, 0.65f, 1.25f);
    float fertMul = std::clamp(0.84f + fertility * 0.24f, 0.75f, 1.15f);
    sample.color *= shade * fertMul;
    if (sample.mountain) {
      sample.accent = glm::clamp(sample.color + glm::vec3(0.14f), glm::vec3(0.0f), glm::vec3(1.0f));
      if (sample.snowCap || h > 0.82f) {
        sample.snowCap = true;
        sample.accent = glm::vec3(0.92f, 0.94f, 0.97f);
      }
    }
    if (sample.material == TerrainMaterialId::Wetlands && isRiver) sample.color = glm::mix(sample.color, material_color(TerrainMaterialId::River), 0.35f);
  }
  sample.color = glm::clamp(sample.color, glm::vec3(0.0f), glm::vec3(1.0f));
  sample.accent = glm::clamp(sample.accent, glm::vec3(0.0f), glm::vec3(1.0f));
  return sample;
}

void reset_terrain_presentation_counters() { gCounters = {}; }
void add_forest_cluster_counter(uint64_t count) { gCounters.forestClusterCount += count; }
const TerrainPresentationCounters& terrain_presentation_counters() { return gCounters; }

} // namespace dom::render
