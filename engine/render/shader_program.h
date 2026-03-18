#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace dom::render {

struct ShaderDebugCounters {
  uint64_t shaderProgramCount{0};
  uint64_t shaderCompileFailures{0};
  uint64_t terrainShaderDraws{0};
  uint64_t modelShaderDraws{0};
  uint64_t shaderFallbackCount{0};
};

class ShaderProgram {
 public:
  ShaderProgram() = default;
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  bool load(std::string_view label, const char* vertexSource, const char* fragmentSource);
  void destroy();

  [[nodiscard]] bool valid() const { return program_ != 0; }
  [[nodiscard]] GLuint id() const { return program_; }
  [[nodiscard]] const std::string& label() const { return label_; }
  [[nodiscard]] const std::string& last_error() const { return lastError_; }

  void bind() const;
  static void unbind();

  GLint uniform_location(const char* name) const;
  void set_mat4(const char* name, const glm::mat4& value) const;
  void set_vec3(const char* name, const glm::vec3& value) const;
  void set_float(const char* name, float value) const;
  void set_int(const char* name, int value) const;

 private:
  GLuint compile_stage(GLenum type, const char* source);

  GLuint program_{0};
  std::string label_;
  std::string lastError_;
};

void reset_shader_debug_counters();
const ShaderDebugCounters& shader_debug_counters();
void note_shader_program_loaded();
void note_shader_compile_failure();
void note_terrain_shader_draw();
void note_model_shader_draw();
void note_shader_fallback();

} // namespace dom::render
