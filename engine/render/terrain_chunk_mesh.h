#pragma once

#include "engine/sim/simulation.h"
#include "engine/render/content_resolution.h"
#include <glm/vec3.hpp>
#include <vector>

namespace dom::render {

struct TerrainChunkVertex {
  float x{0.0f};
  float y{0.0f};
  glm::vec3 color{0.0f};
  glm::vec3 accent{0.0f};
  float slope{0.0f};
  float heightInfluence{0.0f};
  float waterEmphasis{0.0f};
  float blendWeight{0.0f};
  float ambient{0.0f};
  float directional{0.0f};
  float contrast{0.0f};
  float macroVariation{0.0f};
};

struct TerrainChunkMesh {
  int originX{0};
  int originY{0};
  int width{0};
  int height{0};
  std::vector<TerrainChunkVertex> triangles;
};

void build_terrain_chunk_meshes(const dom::sim::World& world, int chunkSize, ContentLodTier lodTier, std::vector<TerrainChunkMesh>& outMeshes);

} // namespace dom::render
