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
  if (setId == "temperate_grass" || setId == "terrain_grass_a" || setId == "terrain_grass_lod0" || setId == "terrain_grass_lod1" || setId == "terrain_grass_lod2") return TerrainMaterialId::Grassland;
  if (setId == "steppe_dry") return TerrainMaterialId::Steppe;
  if (setId == "forest_floor") return TerrainMaterialId::ForestGround;
  if (setId == "sand_dune") return TerrainMaterialId::Desert;
  if (setId == "mediterranean_soil") return TerrainMaterialId::Mediterranean;
  if (setId == "jungle_floor") return TerrainMaterialId::Jungle;
  if (setId == "tundra_soil") return TerrainMaterialId::Tundra;
  if (setId == "snow") return TerrainMaterialId::Snow;
  if (setId == "marsh") return TerrainMaterialId::Wetlands;
  if (setId == "rock_highland" || setId == "mountain_rock_lod0" || setId == "mountain_rock_lod1") return TerrainMaterialId::Mountain;
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
    int nx = x + d[0];
    int ny = y + d[1];
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
    int nx = x + d[0];
    int ny = y + d[1];
    if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height) continue;
    int ni = ny * world.width + nx;
    auto tc = static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(ni)]);
    if (tc == dom::sim::TerrainClass::ShallowWater || tc == dom::sim::TerrainClass::DeepWater) return true;
  }
  return false;
}

float hash01(int x, int y, uint32_t salt) {
  uint32_t h = static_cast<uint32_t>(x) * 0x8da6b343u ^ static_cast<uint32_t>(y) * 0xd8163841u ^ salt * 0xcb1ab31fu;
  h ^= h >> 13;
  h *= 0x85ebca6bu;
  h ^= h >> 16;
  return static_cast<float>(h & 0xffffu) / 65535.0f;
}

glm::vec3 lift_color(const glm::vec3& color, float amount) {
  return glm::mix(color, glm::vec3(1.0f), std::clamp(amount, 0.0f, 1.0f));
}

glm::vec3 shade_color(const glm::vec3& color, float amount) {
  return glm::mix(color, glm::vec3(0.06f, 0.07f, 0.08f), std::clamp(amount, 0.0f, 1.0f));
}

TerrainVisualSample blend_samples(const TerrainVisualSample& a, const TerrainVisualSample& b, float t) {
  TerrainVisualSample out = a;
  const float k = std::clamp(t, 0.0f, 1.0f);
  out.color = glm::mix(a.color, b.color, k);
  out.accent = glm::mix(a.accent, b.accent, k);
  out.ambient = glm::mix(a.ambient, b.ambient, k);
  out.directional = glm::mix(a.directional, b.directional, k);
  out.contrast = glm::mix(a.contrast, b.contrast, k);
  out.terrainBlend = glm::mix(a.terrainBlend, b.terrainBlend, k);
  out.isWater = a.isWater || b.isWater;
  out.hasForestCanopy = a.hasForestCanopy || b.hasForestCanopy;
  out.hasCliff = a.hasCliff || b.hasCliff;
  out.mountain = a.mountain || b.mountain;
  out.snowCap = a.snowCap || b.snowCap;
  if (k > 0.5f) out.material = b.material;
  return out;
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
    int nx = x + d[0];
    int ny = y + d[1];
    if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height) continue;
    int ni = ny * world.width + nx;
    maxDelta = std::max(maxDelta, std::abs(center - world.heightmap[static_cast<size_t>(ni)]));
  }
  return std::clamp(maxDelta * 2.2f, 0.0f, 1.0f);
}

TerrainVisualSample resolve_terrain_visual(const dom::sim::World& world, int cellIndex) {
  TerrainVisualSample sample{};
  if (cellIndex < 0 || cellIndex >= static_cast<int>(world.heightmap.size()) || world.width <= 0 || world.height <= 0) return sample;

  ++gCounters.terrainMaterialResolves;
  const int x = cellIndex % world.width;
  const int y = cellIndex / world.width;
  const float h = world.heightmap[static_cast<size_t>(cellIndex)];
  const float fertility = cellIndex < static_cast<int>(world.fertility.size()) ? world.fertility[static_cast<size_t>(cellIndex)] : 0.5f;
  const auto biome = static_cast<dom::sim::BiomeType>(world.biomeMap[static_cast<size_t>(cellIndex)]);
  const auto tc = static_cast<dom::sim::TerrainClass>(world.terrainClass[static_cast<size_t>(cellIndex)]);
  const bool isRiver = !world.riverMap.empty() && world.riverMap[static_cast<size_t>(cellIndex)] != 0;
  const bool isLake = !world.lakeMap.empty() && world.lakeMap[static_cast<size_t>(cellIndex)] != 0;

  std::string exactId;
  std::string renderClass;
  if (isRiver) {
    exactId = "river";
    renderClass = "river_lake";
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (isLake) {
    exactId = "lake";
    renderClass = "river_lake";
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (tc == dom::sim::TerrainClass::DeepWater) {
    renderClass = "deep_water";
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (tc == dom::sim::TerrainClass::ShallowWater) {
    renderClass = "shallow_water";
    sample.isWater = true;
    ++gCounters.waterFeatureResolves;
  } else if (is_land_coast(world, cellIndex)) {
    renderClass = "coast_littoral";
  } else {
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

  RenderStyleRequest styleRequest{};
  styleRequest.domain = RenderStyleDomain::Terrain;
  styleRequest.exactId = exactId;
  styleRequest.renderClass = renderClass;
  styleRequest.biome = dom::sim::biome_runtime(biome).id;
  styleRequest.lodTier = ContentLodTier::Near;
  const auto style = resolve_render_style(styleRequest);
  if (style.fallback) ++gCounters.presentationFallbackCount;
  sample.material = material_from_set(style.materialSet);
  sample.color = material_color(sample.material);
  sample.accent = sample.color;
  sample.ambient = std::clamp(0.48f + style.readability.ambientBoost, 0.25f, 1.0f);
  sample.directional = std::clamp(0.42f + style.readability.directionalBoost, 0.2f, 1.0f);
  sample.contrast = std::clamp(0.08f + style.readability.stateContrast, 0.0f, 1.0f);
  sample.terrainBlend = std::clamp(style.readability.terrainBlend, 0.0f, 1.0f);
  ++gCounters.terrainLightingSamples;

  const float slope = terrain_slope_hint(world, cellIndex);
  const float waterAdj = neighbor_water_factor(world, cellIndex);
  const float heightBias = std::clamp(h * 0.85f, 0.0f, 0.85f);
  const float directionalBias = std::clamp(0.28f + h * 0.30f - slope * 0.20f + waterAdj * 0.12f + hash01(x, y, 17u) * 0.05f, 0.08f, 0.95f);
  const float ambientBias = std::clamp(0.40f + fertility * 0.14f + waterAdj * 0.18f - slope * 0.10f, 0.22f, 0.92f);
  sample.directional = std::clamp(sample.directional * 0.65f + directionalBias * 0.55f, 0.0f, 1.0f);
  sample.ambient = std::clamp(sample.ambient * 0.58f + ambientBias * 0.62f, 0.0f, 1.0f);

  if (!sample.isWater) {
    const bool cliff = slope > 0.33f && h > 0.45f;
    sample.hasCliff = cliff;
    const float shade = std::clamp(0.82f + heightBias * 0.22f + sample.ambient * 0.18f - slope * 0.14f + waterAdj * 0.06f, 0.62f, 1.18f);
    const float fertMul = std::clamp(0.80f + fertility * 0.28f, 0.72f, 1.12f);
    const float contrastBoost = std::clamp(sample.directional * 0.12f + slope * 0.24f + (sample.mountain ? 0.14f : 0.0f), 0.06f, 0.42f);
    sample.color *= shade * fertMul;
    sample.color = shade_color(sample.color, std::clamp(slope * 0.10f, 0.0f, 0.18f));
    sample.accent = lift_color(sample.color, contrastBoost + style.readability.rimLight * 0.08f);
    sample.contrast = std::clamp(sample.contrast + contrastBoost, 0.0f, 1.0f);
    ++gCounters.terrainContrastSamples;
    ++gCounters.terrainMaterialBlendSamples;

    if (sample.material == TerrainMaterialId::Wetlands && isRiver) sample.color = glm::mix(sample.color, material_color(TerrainMaterialId::River), 0.35f);
    if (sample.material == TerrainMaterialId::Littoral) {
      sample.color = glm::mix(sample.color, material_color(TerrainMaterialId::ShallowOcean), 0.18f);
      sample.accent = glm::mix(sample.accent, material_color(TerrainMaterialId::Desert), 0.12f);
      sample.terrainBlend = std::max(sample.terrainBlend, 0.18f);
    }
    if (sample.mountain) {
      sample.accent = lift_color(sample.accent, 0.08f + slope * 0.18f);
      if (sample.snowCap || h > 0.82f) {
        sample.snowCap = true;
        sample.accent = glm::vec3(0.92f, 0.94f, 0.97f);
        sample.color = glm::mix(sample.color, sample.accent, 0.22f);
      }
    }
  } else {
    sample.ambient = std::clamp(sample.ambient + 0.08f, 0.0f, 1.0f);
    sample.directional = std::clamp(sample.directional + 0.05f, 0.0f, 1.0f);
    sample.contrast = std::clamp(sample.contrast + 0.06f, 0.0f, 1.0f);
    if (sample.material == TerrainMaterialId::River || sample.material == TerrainMaterialId::Lake) {
      sample.accent = glm::mix(sample.color, material_color(TerrainMaterialId::ShallowOcean), 0.35f);
    } else {
      sample.accent = lift_color(sample.color, 0.10f + sample.directional * 0.12f);
    }
  }

  sample.color = glm::clamp(sample.color, glm::vec3(0.0f), glm::vec3(1.0f));
  sample.accent = glm::clamp(sample.accent, glm::vec3(0.0f), glm::vec3(1.0f));
  return sample;
}

TerrainVisualSample resolve_terrain_visual_blended(const dom::sim::World& world, float worldX, float worldY) {
  if (world.width < 1 || world.height < 1 || world.heightmap.empty()) return {};
  const float maxX = static_cast<float>(std::max(0, world.width - 1));
  const float maxY = static_cast<float>(std::max(0, world.height - 1));
  const float fx = std::clamp(worldX, 0.0f, maxX);
  const float fy = std::clamp(worldY, 0.0f, maxY);
  const int x0 = std::clamp(static_cast<int>(fx), 0, world.width - 1);
  const int y0 = std::clamp(static_cast<int>(fy), 0, world.height - 1);
  const int x1 = std::clamp(x0 + 1, 0, world.width - 1);
  const int y1 = std::clamp(y0 + 1, 0, world.height - 1);
  const int i00 = y0 * world.width + x0;
  const int i10 = y0 * world.width + x1;
  const int i01 = y1 * world.width + x0;
  const int i11 = y1 * world.width + x1;

  TerrainVisualSample s00 = resolve_terrain_visual(world, i00);
  TerrainVisualSample s10 = resolve_terrain_visual(world, i10);
  TerrainVisualSample s01 = resolve_terrain_visual(world, i01);
  TerrainVisualSample s11 = resolve_terrain_visual(world, i11);

  float tx = fx - static_cast<float>(x0);
  float ty = fy - static_cast<float>(y0);
  const float jitterX = (hash01(x0, y0, 13u) - 0.5f) * 0.22f;
  const float jitterY = (hash01(x0, y0, 29u) - 0.5f) * 0.22f;
  tx = std::clamp(tx + jitterX, 0.0f, 1.0f);
  ty = std::clamp(ty + jitterY, 0.0f, 1.0f);

  TerrainVisualSample top = blend_samples(s00, s10, tx);
  TerrainVisualSample bottom = blend_samples(s01, s11, tx);
  TerrainVisualSample out = blend_samples(top, bottom, ty);

  const float h = glm::mix(glm::mix(world.heightmap[static_cast<size_t>(i00)], world.heightmap[static_cast<size_t>(i10)], tx),
                           glm::mix(world.heightmap[static_cast<size_t>(i01)], world.heightmap[static_cast<size_t>(i11)], tx), ty);
  if (h > 0.80f) {
    const float snowMix = std::clamp((h - 0.80f) * 2.4f, 0.0f, 1.0f);
    out.accent = glm::mix(out.accent, glm::vec3(0.92f, 0.94f, 0.97f), snowMix);
    out.color = glm::mix(out.color, out.accent, 0.22f + snowMix * 0.12f);
    out.snowCap = true;
  }

  const bool nearWater = s00.isWater || s10.isWater || s01.isWater || s11.isWater;
  if (nearWater && !out.isWater) {
    out.color = glm::mix(out.color, material_color(TerrainMaterialId::Littoral), 0.16f);
    out.terrainBlend = std::max(out.terrainBlend, 0.18f);
  }

  out.color = glm::clamp(out.color, glm::vec3(0.0f), glm::vec3(1.0f));
  out.accent = glm::clamp(out.accent, glm::vec3(0.0f), glm::vec3(1.0f));
  out.ambient = std::clamp(out.ambient, 0.0f, 1.0f);
  out.directional = std::clamp(out.directional, 0.0f, 1.0f);
  out.contrast = std::clamp(out.contrast, 0.0f, 1.0f);
  out.terrainBlend = std::clamp(out.terrainBlend, 0.0f, 1.0f);
  return out;
}

void reset_terrain_presentation_counters() { gCounters = {}; }
void add_forest_cluster_counter(uint64_t count) { gCounters.forestClusterCount += count; }
const TerrainPresentationCounters& terrain_presentation_counters() { return gCounters; }

} // namespace dom::render
