#pragma once

#include <array>
#include <string>

namespace dom::render {

struct ModelShaderParams {
  std::array<float, 3> lightDir{-0.45f, -0.35f, 0.82f};
  std::array<float, 3> baseTint{1.0f, 1.0f, 1.0f};
  float ambient{0.6f};
  float directional{0.7f};
  float rim{0.2f};
  float civTintStrength{0.0f};
  float stateContrast{0.0f};
  float emissiveStrength{0.0f};
  float warningHighlight{0.0f};
  float guardianHighlight{0.0f};
  float industrialHighlight{0.0f};
  float damageContrast{0.0f};
  float terrainBlend{0.0f};
};

bool initialize_model_shader();
bool model_shader_ready();
const std::string& model_shader_status();
void bind_model_shader(const ModelShaderParams& params);
void unbind_model_shader();

} // namespace dom::render
