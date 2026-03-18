#pragma once

#include "engine/render/terrain_chunk_mesh.h"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace dom::render {

bool initialize_terrain_shader();
bool terrain_shader_ready();
const std::string& terrain_shader_status();
bool draw_terrain_chunks_with_shader(const std::vector<TerrainChunkMesh>& chunks,
                                     const glm::mat4& mvp,
                                     const glm::vec3& lightDir,
                                     float overlayProtectionAlpha);

} // namespace dom::render
