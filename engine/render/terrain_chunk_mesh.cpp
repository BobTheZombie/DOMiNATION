#include "engine/render/terrain_chunk_mesh.h"

#include "engine/render/terrain_materials.h"

#include <algorithm>

namespace dom::render {
namespace {

void emit_cell_triangles(const TerrainChunkVertex& v00,
                         const TerrainChunkVertex& v10,
                         const TerrainChunkVertex& v11,
                         const TerrainChunkVertex& v01,
                         std::vector<TerrainChunkVertex>& tris) {
  tris.push_back(v00);
  tris.push_back(v10);
  tris.push_back(v11);

  tris.push_back(v00);
  tris.push_back(v11);
  tris.push_back(v01);
}

} // namespace

void build_terrain_chunk_meshes(const dom::sim::World& world, int chunkSize, ContentLodTier lodTier, std::vector<TerrainChunkMesh>& outMeshes) {
  outMeshes.clear();
  if (world.width < 2 || world.height < 2) return;
  const int step = std::max(4, chunkSize);
  for (int oy = 0; oy < world.height - 1; oy += step) {
    for (int ox = 0; ox < world.width - 1; ox += step) {
      TerrainChunkMesh chunk{};
      chunk.originX = ox;
      chunk.originY = oy;
      chunk.width = std::min(step, (world.width - 1) - ox);
      chunk.height = std::min(step, (world.height - 1) - oy);
      if (chunk.width <= 0 || chunk.height <= 0) continue;

      const int vStride = chunk.width + 1;
      std::vector<TerrainChunkVertex> verts(static_cast<size_t>((chunk.width + 1) * (chunk.height + 1)));
      for (int ly = 0; ly <= chunk.height; ++ly) {
        for (int lx = 0; lx <= chunk.width; ++lx) {
          const float wx = static_cast<float>(ox + lx);
          const float wy = static_cast<float>(oy + ly);
          auto sample = resolve_terrain_visual_blended(world, wx, wy, lodTier);
          auto& v = verts[static_cast<size_t>(ly * vStride + lx)];
          v.x = wx;
          v.y = wy;
          v.color = sample.color;
          v.accent = sample.accent;
          v.slope = sample.slope;
          v.heightInfluence = sample.heightInfluence;
          v.waterEmphasis = sample.waterEmphasis;
          v.blendWeight = sample.terrainBlend;
          v.ambient = sample.ambient;
          v.directional = sample.directional;
          v.contrast = sample.contrast;
          v.macroVariation = sample.macroVariation;
        }
      }

      chunk.triangles.reserve(static_cast<size_t>(chunk.width * chunk.height * 6));
      for (int y = 0; y < chunk.height; ++y) {
        for (int x = 0; x < chunk.width; ++x) {
          const auto& v00 = verts[static_cast<size_t>(y * vStride + x)];
          const auto& v10 = verts[static_cast<size_t>(y * vStride + x + 1)];
          const auto& v11 = verts[static_cast<size_t>((y + 1) * vStride + x + 1)];
          const auto& v01 = verts[static_cast<size_t>((y + 1) * vStride + x)];
          emit_cell_triangles(v00, v10, v11, v01, chunk.triangles);
        }
      }
      outMeshes.push_back(std::move(chunk));
    }
  }
}

} // namespace dom::render
