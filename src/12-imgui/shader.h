#ifndef SHADER_H
#define SHADER_H

#include <glbinding/gl/gl.h>
#include <glbinding/gl/types.h>
#include <glm/glm.hpp>
#include <utility>

class Shader {
public:
  Shader() = default;
  ~Shader();
  Shader(const char *vertex_shader, const char *fragment_shader);

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;

  Shader(Shader &&s) { *this = std::move(s); }
  Shader &operator=(Shader &&s) {
    program = std::move(s.program);
    s.program = 0;
    return *this;
  }

  void use();
  void set_uniform(const char *name, const glm::mat4 &matrix);
  void set_uniform(const char *name, const glm::vec3 &vector);
  void set_uniform(const char *name, const float f);
  gl::GLuint get_program_id() { return program; }

private:
  gl::GLuint program{};
};

Shader get_test_shader();

Shader get_grid_shader();

#endif