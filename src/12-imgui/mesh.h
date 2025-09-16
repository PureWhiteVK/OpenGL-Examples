#ifndef MESH_H
#define MESH_H

#include "drawable.h"

#include <filesystem>
#include <glbinding/gl/gl.h>
#include <glm/glm.hpp>
#include <stdint.h>
#include <vector>

struct Vertex {
  glm::vec3 pos{};
  glm::vec3 normal{};
  glm::vec2 texcoord{};
};

class Mesh : public Drawable {
public:
  void create_buffer() override;
  void destroy_buffer() override;
  void draw() override;

  Mesh() = default;
  ~Mesh() override;

public:
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};
  gl::GLuint vao{}, vbo{}, ebo{};
};

std::vector<Mesh> load_obj_file(std::filesystem::path obj_file);

#endif