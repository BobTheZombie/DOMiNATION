#include "engine/editor/terrain_editor.h"
#include <algorithm>

namespace dom::editor {
void apply_terrain_tool(dom::sim::World& world, int tool, int biome, float delta, int radius, const glm::vec2& center) {
  int cx = std::clamp(static_cast<int>(center.x), 0, world.width - 1);
  int cy = std::clamp(static_cast<int>(center.y), 0, world.height - 1);
  radius = std::max(1, radius);
  for (int y = std::max(0, cy - radius); y <= std::min(world.height - 1, cy + radius); ++y) {
    for (int x = std::max(0, cx - radius); x <= std::min(world.width - 1, cx + radius); ++x) {
      size_t i = static_cast<size_t>(y * world.width + x);
      if (tool == 0) world.biomeMap[i] = static_cast<uint8_t>(std::clamp(biome, 0, static_cast<int>(dom::sim::BiomeType::Count) - 1));
      else if (tool == 1) world.heightmap[i] = std::clamp(world.heightmap[i] + delta, -1.0f, 1.0f);
      else if (tool == 2) {
        world.terrainClass[i] = static_cast<uint8_t>(dom::sim::TerrainClass::ShallowWater);
        world.heightmap[i] = -0.15f;
      }
    }
  }
  world.territoryDirty = true;
  world.fogDirty = true;
}
} // namespace dom::editor
