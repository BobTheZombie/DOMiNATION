#pragma once

#include "engine/sim/simulation.h"
#include <glm/vec3.hpp>
#include <vector>

namespace dom::render {

struct TerrainChunkVertex {
  float x{0.0f};
  float y{0.0f};
  glm::vec3 color{0.0f};
};

struct TerrainChunkMesh {
  int originX{0};
  int originY{0};
  int width{0};
  int height{0};
  std::vector<TerrainChunkVertex> triangles;
};

void build_terrain_chunk_meshes(const dom::sim::World& world, int chunkSize, std::vector<TerrainChunkMesh>& outMeshes);

} // namespace dom::render
