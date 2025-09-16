#include "mesh.h"
#include <filesystem>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glbinding/gl/functions.h>
#include <tiny_obj_loader.h>
#include <unordered_map>

namespace fs = std::filesystem;

using namespace gl;

void Mesh::create_buffer() {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(),
               indices.data(), GL_STATIC_DRAW);

  // position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  // normal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, normal));
  // texcoord
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, texcoord));

  glBindVertexArray(0);
}

void Mesh::destroy_buffer() {
  fmt::println("delete Mesh vao: {}, vbo: {} and ebo: {}", vao, vbo, ebo);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  vao = 0;
  vbo = 0;
  ebo = 0;
}

void Mesh::draw() {
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                 GL_UNSIGNED_INT, 0);
}

Mesh::~Mesh() {}

std::vector<Mesh> load_obj_file(fs::path obj_file) {

  // fs::path obj_file = fs::path("D:\\Download\\Nefertiti\\Nefertiti.obj");
  // fs::path obj_file =
  // fs::path("D:\\Download\\crumpleddevelopable\\CrumpledDevelopable.obj");

  tinyobj::ObjReaderConfig reader_config{};
  reader_config.triangulate = true;
  reader_config.triangulation_method = "earcut";

  tinyobj::ObjReader reader;
  if (!reader.ParseFromFile(obj_file.string(), reader_config)) {
    if (!reader.Error().empty()) {
      fmt::println(stderr, "TinyObjReader: {}", reader.Error());
    }
    return {};
  }

  auto &shapes = reader.GetShapes();

  std::vector<Mesh> meshes{};
  meshes.reserve(shapes.size());
  for (auto &s : shapes) {
    // construct mesh
    Mesh mesh{};
    size_t index_offset = 0;
    // map from tinyobj vertex index to our vertex index
    std::unordered_map<size_t, size_t> vertex_map{};
    mesh.indices.reserve(s.mesh.indices.size());
    for (size_t fv : s.mesh.num_face_vertices) {
      assert(fv == 3); // we requested triangulation
      // we have to reorder the vertices and indices
      for (size_t v = 0; v < fv; v++) {
        tinyobj::index_t idx = s.mesh.indices[index_offset + v];
        if (vertex_map.find(idx.vertex_index) == vertex_map.end()) {
          // not found, add a new vertex
          Vertex vertex{};
          vertex.pos = {
              reader.GetAttrib().vertices[3 * idx.vertex_index + 0],
              reader.GetAttrib().vertices[3 * idx.vertex_index + 1],
              reader.GetAttrib().vertices[3 * idx.vertex_index + 2],
          };
          if (idx.normal_index >= 0) {
            vertex.normal = {
                reader.GetAttrib().normals[3 * idx.normal_index + 0],
                reader.GetAttrib().normals[3 * idx.normal_index + 1],
                reader.GetAttrib().normals[3 * idx.normal_index + 2],
            };
          }
          if (idx.texcoord_index >= 0) {
            vertex.texcoord = {
                reader.GetAttrib().texcoords[2 * idx.texcoord_index + 0],
                reader.GetAttrib().texcoords[2 * idx.texcoord_index + 1],
            };
          }
          mesh.vertices.push_back(vertex);
          size_t new_index = mesh.vertices.size() - 1;
          vertex_map[idx.vertex_index] = new_index;
          mesh.indices.push_back(static_cast<uint32_t>(new_index));
        } else {
          // found, reuse the index
          mesh.indices.push_back(
              static_cast<uint32_t>(vertex_map[idx.vertex_index]));
        }
      }
      if (reader.GetAttrib().normals.empty()) {
        // calculate face normal
        glm::vec3 v0 = mesh.vertices[mesh.indices[index_offset + 0]].pos;
        glm::vec3 v1 = mesh.vertices[mesh.indices[index_offset + 1]].pos;
        glm::vec3 v2 = mesh.vertices[mesh.indices[index_offset + 2]].pos;
        // here we use the unnormalized face normal for weighting
        glm::vec3 face_normal = glm::cross(v1 - v0, v2 - v0);
        for (size_t v = 0; v < fv; v++) {
          mesh.vertices[mesh.indices[index_offset + v]].normal += face_normal;
        }
      }

      index_offset += fv;
    }
    // then we can normalize the normals
    if (reader.GetAttrib().normals.empty()) {
      for (auto &vertex : mesh.vertices) {
        vertex.normal = glm::normalize(vertex.normal);
      }
    }
    meshes.emplace_back(mesh);
  }

  return meshes;
}