#include "primitive.h"
#include "line_loops.h"
#include "lines.h"
#include "mesh.h"

#include <cstdint>
#include <glm/ext.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

// TODO: we have to adjust normal vector here, and change normal to facet like

std::unique_ptr<Mesh> get_icosphere_primitive(int subdivide_level) {
  // start from a regular icosahedron
  const double phi = (1.0 + std::sqrt(5.0)) * 0.5; // golden ratio
  std::vector<glm::dvec3> initial_vertices = {
      {-1, phi, 0}, {1, phi, 0}, {-1, -phi, 0}, {1, -phi, 0},
      {0, -1, phi}, {0, 1, phi}, {0, -1, -phi}, {0, 1, -phi},
      {phi, 0, -1}, {phi, 0, 1}, {-phi, 0, -1}, {-phi, 0, 1},
  };
  for (auto &v : initial_vertices) {
    v = glm::normalize(v);
  }
  std::vector<uint32_t> initial_indices = {
      0, 11, 5,  0, 5,  1, 0, 1, 7, 0, 7,  10, 0, 10, 11, 1, 5, 9, 5, 11,
      4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3,  9,  4, 3,  4,  2, 3, 2, 6, 3,
      6, 8,  3,  8, 9,  4, 9, 5, 2, 4, 11, 6,  2, 10, 8,  6, 7, 9, 8, 1,
  };
  while (subdivide_level > 0) {
    std::vector<glm::dvec3> new_vertices = initial_vertices;
    std::vector<uint32_t> new_indices{};
    std::unordered_map<uint64_t, uint32_t> midpoint_cache{};
    auto get_midpoint_index = [&](uint32_t i0, uint32_t i1) -> uint32_t {
      uint64_t key = (static_cast<uint64_t>(std::min(i0, i1)) << 32) |
                     static_cast<uint64_t>(std::max(i0, i1));
      auto it = midpoint_cache.find(key);
      if (it != midpoint_cache.end()) {
        return it->second;
      }
      glm::dvec3 mid =
          glm::normalize((initial_vertices[i0] + initial_vertices[i1]) * 0.5);
      new_vertices.push_back(mid);
      uint32_t new_index = static_cast<uint32_t>(new_vertices.size() - 1);
      midpoint_cache[key] = new_index;
      return new_index;
    };
    for (size_t i = 0; i < initial_indices.size(); i += 3) {
      uint32_t i0 = initial_indices[i + 0];
      uint32_t i1 = initial_indices[i + 1];
      uint32_t i2 = initial_indices[i + 2];
      uint32_t a = get_midpoint_index(i0, i1);
      uint32_t b = get_midpoint_index(i1, i2);
      uint32_t c = get_midpoint_index(i2, i0);
      new_indices.push_back(i0);
      new_indices.push_back(a);
      new_indices.push_back(c);
      new_indices.push_back(i1);
      new_indices.push_back(b);
      new_indices.push_back(a);
      new_indices.push_back(i2);
      new_indices.push_back(c);
      new_indices.push_back(b);
      new_indices.push_back(a);
      new_indices.push_back(b);
      new_indices.push_back(c);
    }
    initial_vertices = std::move(new_vertices);
    initial_indices = std::move(new_indices);
    subdivide_level--;
  }
  // convert to float and construct Mesh
  std::vector<Vertex> vertices{};
  vertices.reserve(initial_vertices.size());
  for (auto &v : initial_vertices) {
    Vertex vertex{};
    vertex.pos = glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y),
                           static_cast<float>(v.z));
    vertex.normal = vertex.pos;
    vertex.texcoord = glm::vec2((v.x + 1.0f) * 0.5f, (v.y + 1.0f) * 0.5f);
    vertices.push_back(vertex);
  }
  std::vector<uint32_t> indices = std::move(initial_indices);
  std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
  mesh->vertices = std::move(vertices);
  mesh->indices = std::move(indices);
  return mesh;
}

std::unique_ptr<Mesh> get_simplex_disk_primitive(int subdivide_level) {
  // 基本思想就是，用正三角形逼近 1/6 圆，通过不断递归的 subdivision
  // 步骤：先进行曲面细分，然后调整顶点位置，使其接近圆
  // start from two regular triangle
  static const double inv_sqrt3 = std::sqrt(3) / 3;
  static const double sqrt3 = std::sqrt(3.0);
  static const double half_sqrt3 = 0.5 * sqrt3;
  // triangle vertices, in counter-clockwise order
  glm::dvec2 p0{1.0, 0.0}, p1{0.5, half_sqrt3};
  std::vector<glm::dvec2> initial_vertices = {
      {0.0, 0.0},  {1.0, 0.0},          {0.5, half_sqrt3}, {-0.5, half_sqrt3},
      {-1.0, 0.0}, {-0.5, -half_sqrt3}, {0.5, -half_sqrt3}};
  std::vector<uint32_t> initial_indices = {0, 1, 2, 0, 2, 3, 0, 3, 4,
                                           0, 4, 5, 0, 5, 6, 0, 6, 1};

  for (int i = 0; i < subdivide_level; i++) {
    std::vector<glm::dvec2> new_vertices = initial_vertices;
    std::vector<uint32_t> new_indices{};
    std::vector<uint32_t> new_status{};
    std::unordered_map<uint64_t, uint32_t> midpoint_cache{};
    auto get_midpoint_index = [&](uint32_t i0, uint32_t i1) -> uint32_t {
      uint64_t key = (static_cast<uint64_t>(std::min(i0, i1)) << 32) |
                     static_cast<uint64_t>(std::max(i0, i1));
      auto it = midpoint_cache.find(key);
      if (it != midpoint_cache.end()) {
        return it->second;
      }
      // here we should project the midpoint to another point, not unit circle
      // glm::dvec2 mid = (initial_vertices[i0] + initial_vertices[i1]) * 0.5;
      // here if initial_vertices[i0] and initial_vertices[i1] pass the origin,
      // we should keep the mid point
      glm::dvec2 v0 = initial_vertices[i0];
      glm::dvec2 v1 = initial_vertices[i1];
      glm::dvec2 mid = (v0 + v1) * 0.5;
      new_vertices.push_back(mid);
      uint32_t new_index = static_cast<uint32_t>(new_vertices.size() - 1);
      midpoint_cache[key] = new_index;
      return new_index;
    };
    for (uint32_t i = 0; i < initial_indices.size(); i += 3) {
      uint32_t i0 = initial_indices[i + 0];
      uint32_t i1 = initial_indices[i + 1];
      uint32_t i2 = initial_indices[i + 2];
      uint32_t a = get_midpoint_index(i0, i1);
      uint32_t b = get_midpoint_index(i1, i2);
      uint32_t c = get_midpoint_index(i2, i0);

      // (0,0)
      new_indices.push_back(i0);
      new_indices.push_back(a);
      new_indices.push_back(c);
      // (0,1)
      new_indices.push_back(a);
      new_indices.push_back(i1);
      new_indices.push_back(b);
      // (1,0)
      new_indices.push_back(c);
      new_indices.push_back(b);
      new_indices.push_back(i2);
      // (1,1)
      new_indices.push_back(a);
      new_indices.push_back(b);
      new_indices.push_back(c);
    }

    initial_vertices = std::move(new_vertices);
    initial_indices = std::move(new_indices);
  }
  // convert to float and construct Mesh
  std::vector<Vertex> vertices{};
  vertices.reserve(initial_vertices.size());
  uint32_t vertex_idx = 0;

  for (auto &v : initial_vertices) {
    Vertex vertex{};
    // calculate the length to original line
    // double dist_to_line =
    //     (p1.x - p0.x) * (v.y - p0.y) - (p1.y - p0.y) * (v.x - p0.x);
    // double k = dist_to_line * 2.0 * inv_sqrt3;
    // calculate current point to y = 0, y = sqrt(3) * x, y = -sqrt(3) * x
    double k0 = v.y;
    double k1 = 0.5 * (v.x * sqrt3 - v.y);
    double k2 = 0.5 * (-v.x * sqrt3 - v.y);
    uint32_t case_id = (static_cast<uint32_t>(std::signbit(k0)) << 2) |
                       (static_cast<uint32_t>(std::signbit(k1)) << 1) |
                       static_cast<uint32_t>(std::signbit(k2));
    static constexpr std::array<uint32_t, 8> case_map{3, 2, 1, 0, 0, 1, 2, 3};
    std::array<double, 4> k{
        k0,
        k1,
        k2,
        -1.0,
    };
    glm::dvec2 adjusted_v{};
    if (std::any_of(k.begin(), k.end(), [](double v) -> bool {
          return glm::epsilonEqual(v, 0.0, 1e-9);
        })) {
      adjusted_v = v;
    } else {
      adjusted_v = glm::normalize(v) * std::abs(k[case_map[case_id]]) * (2.0) *
                   inv_sqrt3;
    }
    vertex.pos = glm::vec3(static_cast<float>(adjusted_v.x),
                           static_cast<float>(adjusted_v.y), 0.0f);
    vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertex.texcoord = glm::vec2((v.x + 1.0f) * 0.5f, (v.y + 1.0f) * 0.5f);
    vertices.push_back(vertex);
    vertex_idx += 1;
  }
  std::vector<uint32_t> indices = std::move(initial_indices);
  std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
  mesh->vertices = std::move(vertices);
  mesh->indices = std::move(indices);
  return mesh;
}

void get_subdivide_circles(int subdivide_level,
                           std::vector<glm::dvec2> &vertices) {
  // start from a regular triangle
  const double sqrt3 = std::sqrt(3.0);
  // triangle vertices, in counter-clockwise order
  std::vector<glm::dvec2> initial_vertices = {
      {0.0, 1.0}, {-sqrt3 / 2.0, -0.5}, {sqrt3 / 2.0, -0.5}};
  while (subdivide_level > 0) {
    std::vector<glm::dvec2> new_vertices{};
    for (size_t i = 0; i < initial_vertices.size(); i++) {
      glm::dvec2 v0 = initial_vertices[i];
      glm::dvec2 v1 = initial_vertices[(i + 1) % initial_vertices.size()];
      glm::dvec2 mid = (v0 + v1) * 0.5;
      // project to unit circle
      mid = glm::normalize(mid);
      new_vertices.push_back(v0);
      new_vertices.push_back(mid);
    }
    initial_vertices = std::move(new_vertices);
    subdivide_level--;
  }
  vertices = std::move(initial_vertices);
}

std::unique_ptr<Mesh> get_disk_primitive(int subdivide_level) {
  std::vector<glm::dvec2> initial_vertices{};
  get_subdivide_circles(subdivide_level, initial_vertices);

  std::vector<Vertex> vertices{};
  vertices.resize(initial_vertices.size() + 1);
  uint32_t last_idx = static_cast<uint32_t>(initial_vertices.size());
  std::vector<uint32_t> indices{};
  for (int i = 0; i < initial_vertices.size(); i++) {
    uint32_t i0 = i;
    uint32_t i1 = (i + 1) % initial_vertices.size();
    indices.emplace_back(last_idx);
    indices.emplace_back(i0);
    indices.emplace_back(i1);
    vertices[i].pos = glm::vec3(initial_vertices[i0], 0.0f);
    vertices[i].normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[i].texcoord = glm::vec2((initial_vertices[i0].x + 1.0f) * 0.5f,
                                     (initial_vertices[i0].y + 1.0f) * 0.5f);
  }
  vertices[last_idx] =
      (Vertex{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f}});
  std::unique_ptr<Mesh> m = std::make_unique<Mesh>();
  m->vertices = std::move(vertices);
  m->indices = std::move(indices);
  return m;
}

std::unique_ptr<LineLoops> get_circle_primitive(int subdivide_level) {
  std::vector<glm::dvec2> initial_vertices{};
  get_subdivide_circles(subdivide_level, initial_vertices);
  std::vector<glm::dvec2> vertices = std::move(initial_vertices);
  std::vector<Vertex> line_loops{};
  line_loops.resize(vertices.size());
  for (int i = 0; i < vertices.size(); i++) {
    Vertex v{};
    v.pos = glm::vec3(vertices[i], 0.0f);
    v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    line_loops[i] = v;
  }

  std::unique_ptr<LineLoops> loops = std::make_unique<LineLoops>();
  loops->vertices = std::move(line_loops);
  return loops;
}

std::unique_ptr<Mesh> get_polygon_cone_primitive(int subdivide_level) {
  std::vector<glm::dvec2> initial_vertices{};
  get_subdivide_circles(subdivide_level, initial_vertices);
  glm::vec3 center_coord = {0.0f, 0.0f, -1.0f};
  glm::vec3 apex_coord = {0.0f, 0.0f, 1.0f};
  std::vector<Vertex> vertices{};
  uint32_t center_idx = vertices.size();
  vertices.emplace_back(Vertex{center_coord, glm::vec3{0.0f, 0.0f, -1.0f}});
  std::vector<uint32_t> indices{};

  for (int i = 0; i < initial_vertices.size(); i++) {
    glm::dvec2 &dv0 = initial_vertices[i];
    glm::dvec2 &dv1 = initial_vertices[(i + 1) % (initial_vertices.size())];
    glm::vec3 v0 = {dv0.x, dv0.y, -1.0f};
    glm::vec3 v1 = {dv1.x, dv1.y, -1.0f};
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, apex_coord - v0));
    // calculate face normal for slope triangle
    indices.emplace_back(static_cast<uint32_t>(vertices.size()));
    vertices.emplace_back(Vertex{v0, normal});
    indices.emplace_back(static_cast<uint32_t>(vertices.size()));
    vertices.emplace_back(Vertex{v1, normal});
    indices.emplace_back(static_cast<uint32_t>(vertices.size()));
    vertices.emplace_back(Vertex{apex_coord, normal});
    // bottom face
    indices.emplace_back(static_cast<uint32_t>(vertices.size()));
    vertices.emplace_back(Vertex{v0, glm::vec3{0.0f, 0.0f, -1.0f}});
    indices.emplace_back(static_cast<uint32_t>(vertices.size()));
    vertices.emplace_back(Vertex{v1, glm::vec3{0.0f, 0.0f, -1.0f}});
    indices.emplace_back(center_idx);
  }

  std::unique_ptr<Mesh> m = std::make_unique<Mesh>();
  m->vertices = std::move(vertices);
  m->indices = std::move(indices);
  return m;
}

std::unique_ptr<Mesh> get_cylinder_primitive(int subdivide_level) {
  std::vector<glm::dvec2> initial_vertices{};
  get_subdivide_circles(subdivide_level, initial_vertices);

  std::vector<Vertex> vertices{};
  vertices.reserve(initial_vertices.size() * 4 + 2);
  std::vector<uint32_t> indices{};
  for (int i = 0; i < initial_vertices.size(); i++) {
    int j = (i + 1) % (initial_vertices.size());
    glm::dvec2 &dv0 = initial_vertices[i];
    glm::dvec2 &dv1 = initial_vertices[j];

    glm::vec3 v0 = {dv0.x, dv0.y, -1.0f};
    glm::vec3 v2 = {dv0.x, dv0.y, 1.0f};
    glm::vec3 n0 = {dv0.x, dv0.y, 0.0f};

    vertices.emplace_back(Vertex{v0, n0});
    vertices.emplace_back(Vertex{v2, n0});

    uint32_t i0 = 2 * i;
    uint32_t i2 = i0 + 1;
    uint32_t i1 = 2 * j;
    uint32_t i3 = i1 + 1;

    indices.emplace_back(i0);
    indices.emplace_back(i1);
    indices.emplace_back(i3);

    indices.emplace_back(i0);
    indices.emplace_back(i3);
    indices.emplace_back(i2);
  }

  std::unique_ptr<Mesh> disk_face = get_disk_primitive(subdivide_level);

  uint32_t index_offset = vertices.size();
  for (auto &v : disk_face->vertices) {
    vertices.emplace_back(Vertex{v.pos + glm::vec3{0.0f, 0.0f, 1.0f},
                                 glm::vec3{0.0f, 0.0f, 1.0f}});
  }
  for (auto i : disk_face->indices) {
    indices.emplace_back(i + index_offset);
  }

  index_offset = vertices.size();
  for (auto &v : disk_face->vertices) {
    vertices.emplace_back(Vertex{v.pos + glm::vec3{0.0f, 0.0f, -1.0f},
                                 glm::vec3{0.0f, 0.0f, -1.0f}});
  }
  for (auto i : disk_face->indices) {
    indices.emplace_back(i + index_offset);
  }

  std::unique_ptr<Mesh> m = std::make_unique<Mesh>();
  m->vertices = std::move(vertices);
  m->indices = std::move(indices);
  return m;
}

std::unique_ptr<Mesh> get_cube_primitive() {
  std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
  mesh->indices = {// +X face
                   0, 1, 2, 0, 2, 3,
                   // -X face
                   4, 5, 6, 4, 6, 7,
                   // +Y face
                   8, 9, 10, 8, 10, 11,
                   // -Y face
                   12, 13, 14, 12, 14, 15,
                   // +Z face
                   16, 17, 18, 16, 18, 19,
                   // -Z face
                   20, 21, 22, 20, 22, 23};
  mesh->vertices = {
      // +X face
      {{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0.0f, 0.0f}}, // 0
      {{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {1.0f, 0.0f}},  // 1
      {{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {1.0f, 1.0f}},   // 2
      {{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {0.0f, 1.0f}},  // 3

      // -X face
      {{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {0.0f, 0.0f}},  // 4
      {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {1.0f, 0.0f}}, // 5
      {{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {1.0f, 1.0f}},  // 6
      {{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {0.0f, 1.0f}},   // 7

      // +Y face
      {{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0.0f, 0.0f}}, // 8
      {{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1.0f, 0.0f}},  // 9
      {{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1.0f, 1.0f}},   // 10
      {{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0.0f, 1.0f}},  // 11

      // -Y face
      {{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0.0f, 0.0f}},  // 12
      {{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1.0f, 0.0f}},   // 13
      {{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1.0f, 1.0f}},  // 14
      {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0.0f, 1.0f}}, // 15

      // +Z face
      {{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0.0f, 0.0f}}, // 16
      {{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1.0f, 0.0f}},  // 17
      {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1.0f, 1.0f}},   // 18
      {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0.0f, 1.0f}},  // 19

      // -Z face
      {{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0.0f, 0.0f}},  // 20
      {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1.0f, 0.0f}}, // 21
      {{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1.0f, 1.0f}},  // 22
      {{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0.0f, 1.0f}},   // 23
  };
  return mesh;
}

std::unique_ptr<Mesh> get_plane_primitive() {
  std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
  mesh->indices = {0, 1, 2, 0, 2, 3};
  mesh->vertices = {
      {glm::vec3{-1.0f, 0.0f, -1.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
       glm::vec2{0.0f, 0.0f}},
      {glm::vec3{1.0f, 0.0f, -1.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
       glm::vec2{1.0f, 0.0f}},
      {glm::vec3{1.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
       glm::vec2{1.0f, 1.0f}},
      {glm::vec3{-1.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
       glm::vec2{0.0f, 1.0f}},
  };
  return mesh;
}

std::unique_ptr<Lines> get_cube_wireframe_primitive() {
  std::vector<uint32_t> indices = {// 底面
                                   0, 1, 1, 2, 2, 3, 3, 0,
                                   // 顶面
                                   4, 5, 5, 6, 6, 7, 7, 4,
                                   // 侧面
                                   0, 4, 1, 5, 2, 6, 3, 7};

  std::vector<Vertex> vertices = {
      {{-1.0f, -1.0f, -1.0f}}, // 0
      {{1.0f, -1.0f, -1.0f}},  // 1
      {{1.0f, 1.0f, -1.0f}},   // 2
      {{-1.0f, 1.0f, -1.0f}},  // 3
      {{-1.0f, -1.0f, 1.0f}},  // 4
      {{1.0f, -1.0f, 1.0f}},   // 5
      {{1.0f, 1.0f, 1.0f}},    // 6
      {{-1.0f, 1.0f, 1.0f}},   // 7
  };

  std::unique_ptr<Lines> lines = std::make_unique<Lines>();
  lines->vertices = vertices;
  lines->indices = indices;
  return lines;
}