#include "engine/render/terrain_shader.h"

#include "engine/render/shader_program.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace dom::render {
namespace {
ShaderProgram gTerrainShader{};
bool gTerrainShaderInitialized = false;
std::string gTerrainShaderStatus{"terrain shader pending initialization"};

constexpr const char* kTerrainVertexShader = R"GLSL(
#version 120
uniform mat4 uMvp;
varying vec3 vBaseColor;
varying vec3 vAccentColor;
varying vec4 vSurfaceParams;
varying vec4 vLightingParams;
varying vec2 vWorldPos;
void main() {
  gl_Position = uMvp * gl_Vertex;
  vBaseColor = gl_Color.rgb;
  vAccentColor = gl_MultiTexCoord2.xyz;
  vSurfaceParams = gl_MultiTexCoord0;
  vLightingParams = gl_MultiTexCoord1;
  vWorldPos = gl_Vertex.xy;
}
)GLSL";

constexpr const char* kTerrainFragmentShader = R"GLSL(
#version 120
uniform vec3 uLightDir;
uniform float uOverlayProtection;
varying vec3 vBaseColor;
varying vec3 vAccentColor;
varying vec4 vSurfaceParams;
varying vec4 vLightingParams;
varying vec2 vWorldPos;

float macro_noise(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
  float slope = clamp(vSurfaceParams.x, 0.0, 1.0);
  float heightInfluence = clamp(vSurfaceParams.y, 0.0, 1.0);
  float waterEmphasis = clamp(vSurfaceParams.z, 0.0, 1.0);
  float blend = clamp(vSurfaceParams.w, 0.0, 1.0);
  float ambient = clamp(vLightingParams.x, 0.0, 1.2);
  float directional = clamp(vLightingParams.y, 0.0, 1.2);
  float contrast = clamp(vLightingParams.z, 0.0, 1.0);
  float macroStrength = clamp(vLightingParams.w, 0.0, 1.0);

  vec3 lit = mix(vBaseColor, vAccentColor, blend * (0.32 + contrast * 0.34));
  float slopeLight = clamp(1.0 - slope * (0.75 - directional * 0.18), 0.25, 1.2);
  float macro = (macro_noise(vWorldPos * 0.145) - 0.5) * (0.12 + macroStrength * 0.24);
  float ridgeLift = heightInfluence * 0.16 + directional * 0.18;
  float basinShade = slope * 0.10 + (1.0 - ambient) * 0.12;
  vec3 terrain = lit * (0.78 + ambient * 0.26 + ridgeLift - basinShade + macro);
  terrain = mix(terrain, vAccentColor, directional * 0.12 + heightInfluence * 0.08);
  vec3 waterTint = mix(vec3(0.10, 0.26, 0.46), vec3(0.34, 0.62, 0.86), directional * 0.35 + ambient * 0.20);
  terrain = mix(terrain, waterTint, waterEmphasis * 0.18);
  float overlayPreserve = clamp(uOverlayProtection, 0.3, 1.0);
  terrain = mix(terrain, vAccentColor, contrast * 0.06 * overlayPreserve);
  gl_FragColor = vec4(clamp(terrain, 0.0, 1.0), 1.0);
}
)GLSL";
} // namespace

bool initialize_terrain_shader() {
  if (gTerrainShaderInitialized) return gTerrainShader.valid();
  gTerrainShaderInitialized = true;
  if (gTerrainShader.load("terrain", kTerrainVertexShader, kTerrainFragmentShader)) {
    gTerrainShaderStatus = "terrain shader active";
    return true;
  }
  gTerrainShaderStatus = gTerrainShader.last_error().empty() ? "terrain shader unavailable" : gTerrainShader.last_error();
  note_shader_fallback();
  return false;
}

bool terrain_shader_ready() { return initialize_terrain_shader() && gTerrainShader.valid(); }
const std::string& terrain_shader_status() { initialize_terrain_shader(); return gTerrainShaderStatus; }

bool draw_terrain_chunks_with_shader(const std::vector<TerrainChunkMesh>& chunks,
                                     const glm::mat4& mvp,
                                     const glm::vec3& lightDir,
                                     float overlayProtectionAlpha) {
  if (!terrain_shader_ready()) return false;
  gTerrainShader.bind();
  gTerrainShader.set_mat4("uMvp", mvp);
  gTerrainShader.set_vec3("uLightDir", glm::normalize(lightDir));
  gTerrainShader.set_float("uOverlayProtection", overlayProtectionAlpha);

  note_terrain_shader_draw();
  glBegin(GL_TRIANGLES);
  for (const auto& chunk : chunks) {
    for (const auto& v : chunk.triangles) {
      glColor3f(v.color.r, v.color.g, v.color.b);
      glMultiTexCoord4f(GL_TEXTURE0, v.slope, v.heightInfluence, v.waterEmphasis, v.blendWeight);
      glMultiTexCoord4f(GL_TEXTURE1, v.ambient, v.directional, v.contrast, v.macroVariation);
      glMultiTexCoord3f(GL_TEXTURE2, v.accent.r, v.accent.g, v.accent.b);
      glVertex3f(v.x, v.y, 0.0f);
    }
  }
  glEnd();
  ShaderProgram::unbind();
  return true;
}

} // namespace dom::render
