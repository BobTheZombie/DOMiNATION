#include "engine/render/model_shader.h"

#include "engine/render/shader_program.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace dom::render {
namespace {
ShaderProgram gModelShader{};
bool gModelShaderInitialized = false;
std::string gModelShaderStatus{"model shader pending initialization"};

constexpr const char* kModelVertexShader = R"GLSL(
#version 120
uniform mat4 uMvp;
varying vec3 vColor;
varying vec3 vNormal;
varying vec3 vWorldPos;
void main() {
  gl_Position = uMvp * gl_Vertex;
  vColor = gl_Color.rgb;
  vNormal = normalize(gl_NormalMatrix * gl_Normal);
  vWorldPos = gl_Vertex.xyz;
}
)GLSL";

constexpr const char* kModelFragmentShader = R"GLSL(
#version 120
uniform vec3 uLightDir;
uniform vec3 uBaseTint;
uniform float uAmbient;
uniform float uDirectional;
uniform float uRim;
uniform float uCivTintStrength;
uniform float uStateContrast;
uniform float uEmissiveStrength;
uniform float uWarningHighlight;
uniform float uGuardianHighlight;
uniform float uIndustrialHighlight;
uniform float uDamageContrast;
uniform float uTerrainBlend;
varying vec3 vColor;
varying vec3 vNormal;
varying vec3 vWorldPos;
void main() {
  vec3 n = normalize(vNormal);
  vec3 l = normalize(uLightDir);
  float ndl = clamp(dot(n, l) * 0.5 + 0.5, 0.0, 1.0);
  float rim = pow(clamp(1.0 - n.z, 0.0, 1.0), 1.6) * uRim;
  vec3 color = vColor * mix(vec3(1.0), uBaseTint, clamp(uCivTintStrength, 0.0, 1.0) * 0.22);
  color *= (0.62 + uAmbient * 0.48 + ndl * uDirectional * 0.42);
  color = mix(color, vec3(dot(color, vec3(0.299, 0.587, 0.114))), clamp(uDamageContrast, 0.0, 1.0) * 0.14);
  color = mix(color, vec3(1.0), rim * (0.18 + uStateContrast * 0.18));
  vec3 emphasis = vec3(1.0, 0.38, 0.24) * uWarningHighlight + vec3(0.90, 0.50, 0.98) * uGuardianHighlight + vec3(0.96, 0.72, 0.34) * uIndustrialHighlight;
  color = mix(color, color + emphasis, clamp(uStateContrast * 0.12 + uTerrainBlend * 0.08, 0.0, 0.35));
  float band = smoothstep(0.10, 0.38, fract(max(vWorldPos.z, 0.0) * 0.85 + 0.18));
  color += emphasis * (0.06 + uEmissiveStrength * 0.20) * band;
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)GLSL";

glm::mat4 current_mvp() {
  glm::mat4 projection{1.0f};
  glm::mat4 modelView{1.0f};
  glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
  glGetFloatv(GL_MODELVIEW_MATRIX, &modelView[0][0]);
  return projection * modelView;
}
} // namespace

bool initialize_model_shader() {
  if (gModelShaderInitialized) return gModelShader.valid();
  gModelShaderInitialized = true;
  if (gModelShader.load("model", kModelVertexShader, kModelFragmentShader)) {
    gModelShaderStatus = "model shader active";
    return true;
  }
  gModelShaderStatus = gModelShader.last_error().empty() ? "model shader unavailable" : gModelShader.last_error();
  note_shader_fallback();
  return false;
}

bool model_shader_ready() { return initialize_model_shader() && gModelShader.valid(); }
const std::string& model_shader_status() { initialize_model_shader(); return gModelShaderStatus; }

void bind_model_shader(const ModelShaderParams& params) {
  if (!model_shader_ready()) return;
  gModelShader.bind();
  gModelShader.set_mat4("uMvp", current_mvp());
  gModelShader.set_vec3("uLightDir", glm::normalize(glm::vec3(params.lightDir[0], params.lightDir[1], params.lightDir[2])));
  gModelShader.set_vec3("uBaseTint", glm::vec3(params.baseTint[0], params.baseTint[1], params.baseTint[2]));
  gModelShader.set_float("uAmbient", params.ambient);
  gModelShader.set_float("uDirectional", params.directional);
  gModelShader.set_float("uRim", params.rim);
  gModelShader.set_float("uCivTintStrength", params.civTintStrength);
  gModelShader.set_float("uStateContrast", params.stateContrast);
  gModelShader.set_float("uEmissiveStrength", params.emissiveStrength);
  gModelShader.set_float("uWarningHighlight", params.warningHighlight);
  gModelShader.set_float("uGuardianHighlight", params.guardianHighlight);
  gModelShader.set_float("uIndustrialHighlight", params.industrialHighlight);
  gModelShader.set_float("uDamageContrast", params.damageContrast);
  gModelShader.set_float("uTerrainBlend", params.terrainBlend);
  note_model_shader_draw();
}

void unbind_model_shader() { ShaderProgram::unbind(); }

} // namespace dom::render
