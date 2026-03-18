#include "engine/render/shader_program.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <iostream>
#include <vector>

namespace dom::render {
namespace {
ShaderDebugCounters gShaderCounters{};

std::string shader_log(GLuint object, bool program) {
  GLint length = 0;
  if (program) glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
  else glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
  if (length <= 1) return {};
  std::vector<GLchar> buffer(static_cast<size_t>(length), 0);
  if (program) glGetProgramInfoLog(object, length, nullptr, buffer.data());
  else glGetShaderInfoLog(object, length, nullptr, buffer.data());
  return std::string(buffer.data());
}
} // namespace

ShaderProgram::~ShaderProgram() { destroy(); }

GLuint ShaderProgram::compile_stage(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  if (shader == 0) {
    lastError_ = label_ + ": unable to allocate shader object";
    return 0;
  }
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) return shader;
  lastError_ = label_ + ": compile failed: " + shader_log(shader, false);
  glDeleteShader(shader);
  shader = 0;
  note_shader_compile_failure();
  std::cerr << "[shader] " << lastError_ << '\n';
  return 0;
}

bool ShaderProgram::load(std::string_view label, const char* vertexSource, const char* fragmentSource) {
  destroy();
  label_ = std::string(label);
  lastError_.clear();

  GLuint vs = compile_stage(GL_VERTEX_SHADER, vertexSource);
  if (vs == 0) return false;
  GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fragmentSource);
  if (fs == 0) {
    glDeleteShader(vs);
    return false;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    lastError_ = label_ + ": link failed: " + shader_log(program_, true);
    note_shader_compile_failure();
    std::cerr << "[shader] " << lastError_ << '\n';
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }

  note_shader_program_loaded();
  return true;
}

void ShaderProgram::destroy() {
  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }
}

void ShaderProgram::bind() const {
  if (program_ != 0) glUseProgram(program_);
}

void ShaderProgram::unbind() { glUseProgram(0); }

GLint ShaderProgram::uniform_location(const char* name) const {
  if (program_ == 0) return -1;
  return glGetUniformLocation(program_, name);
}

void ShaderProgram::set_mat4(const char* name, const glm::mat4& value) const {
  const GLint loc = uniform_location(name);
  if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::set_vec3(const char* name, const glm::vec3& value) const {
  const GLint loc = uniform_location(name);
  if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(value));
}

void ShaderProgram::set_float(const char* name, float value) const {
  const GLint loc = uniform_location(name);
  if (loc >= 0) glUniform1f(loc, value);
}

void ShaderProgram::set_int(const char* name, int value) const {
  const GLint loc = uniform_location(name);
  if (loc >= 0) glUniform1i(loc, value);
}

void reset_shader_debug_counters() {
  gShaderCounters.terrainShaderDraws = 0;
  gShaderCounters.modelShaderDraws = 0;
  gShaderCounters.shaderFallbackCount = 0;
}
const ShaderDebugCounters& shader_debug_counters() { return gShaderCounters; }
void note_shader_program_loaded() { ++gShaderCounters.shaderProgramCount; }
void note_shader_compile_failure() { ++gShaderCounters.shaderCompileFailures; }
void note_terrain_shader_draw() { ++gShaderCounters.terrainShaderDraws; }
void note_model_shader_draw() { ++gShaderCounters.modelShaderDraws; }
void note_shader_fallback() { ++gShaderCounters.shaderFallbackCount; }

} // namespace dom::render
