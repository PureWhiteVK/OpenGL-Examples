#include <algorithm>
#include <assert.h>
#include <cmath>
#include <cstdint>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/types.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stddef.h>
#include <stdio.h>

#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include <fmt/base.h>
#include <fmt/ostream.h>

#include <glbinding-aux/ContextInfo.h>
#include <glbinding-aux/types_to_string.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <tiny_obj_loader.h>

const char *vertex_shader_text = R"(\
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	TexCoord = vec2(aTexCoord.x, aTexCoord.y);
  Normal = aNormal;
}
)";

const char *fragment_shader_text = R"(\
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
// uniform sampler2D texture1;
// uniform sampler2D texture2;

void main()
{
	// FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
  // FragColor = vec4(1.0, 0.5, 0.2, 1.0);
  FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0);
}
)";

using namespace gl;

struct Vertex {
  glm::vec3 pos{};
  glm::vec3 normal{};
  glm::vec2 texcoord{};
};

struct Mesh {
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};
};

template <> struct fmt::formatter<GLenum> : fmt::ostream_formatter {};

std::vector<Mesh> load_obj_file() {
  namespace fs = std::filesystem;

  fs::path obj_file = fs::path("D:\\Download\\Nefertiti\\Nefertiti.obj");
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
      // calculate face normal
      glm::vec3 v0 = mesh.vertices[mesh.indices[index_offset + 0]].pos;
      glm::vec3 v1 = mesh.vertices[mesh.indices[index_offset + 1]].pos;
      glm::vec3 v2 = mesh.vertices[mesh.indices[index_offset + 2]].pos;
      // here we use the unnormalized face normal for weighting
      glm::vec3 face_normal = glm::cross(v1 - v0, v2 - v0);
      for (size_t v = 0; v < fv; v++) {
        mesh.vertices[mesh.indices[index_offset + v]].normal += face_normal;
      }
      index_offset += fv;
    }
    // then we can normalize the normals
    for (auto &vertex : mesh.vertices) {
      vertex.normal = glm::normalize(vertex.normal);
    }
    meshes.emplace_back(mesh);
  }

  return meshes;
}

using KeyCallback = std::function<void(GLFWwindow *, int, int, int, int)>;

std::vector<KeyCallback> key_callbacks{};

void key_callback_dispatcher(GLFWwindow *window, int key, int scancode,
                             int action, int mods) {
  for (auto &cb : key_callbacks) {
    cb(window, key, scancode, action, mods);
  }
}

GLFWwindow *init_window(int width, int height) {
  glfwSetErrorCallback([](int error, const char *description) {
    fmt::println(stderr, "Glfw Error: {}", description);
  });

  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  // high dpi support
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

  // Request an OpenGL 4.2 core profile context
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(width, height, "OpenGL ObjLoader", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetWindowSizeCallback(window,
                            [](GLFWwindow *window, int width, int height) {
                              gl::glViewport(0, 0, width, height);
                            });

  glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode,
                                int action, int mods) {
    key_callback_dispatcher(window, key, scancode, action, mods);
  });

  glfwMakeContextCurrent(window);
  glbinding::initialize(glfwGetProcAddress);

  fmt::println("OpenGL Version: {}",
               glbinding::aux::ContextInfo::version().toString());

  glDebugMessageCallback(
      [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
         const GLchar *message, const void *userParam) {
        fmt::println(
            "[OpenGL Debug] source = {}, type = {}, id = {}, severity = {}, "
            "message = {}",
            source, type, id, severity, message);
      },
      nullptr);

  // Enable vsync
  glfwSwapInterval(1);

  return window;
}

void destroy_window(GLFWwindow *window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}

void create_buffers(std::vector<Mesh> &meshes, std::vector<GLuint> &vaos,
                    std::vector<GLuint> &vbos, std::vector<GLuint> &ebos) {
  vaos.resize(meshes.size());
  vbos.resize(meshes.size());
  ebos.resize(meshes.size());
  glGenVertexArrays(static_cast<GLsizei>(meshes.size()), vaos.data());
  glGenBuffers(static_cast<GLsizei>(meshes.size()), vbos.data());
  glGenBuffers(static_cast<GLsizei>(meshes.size()), ebos.data());

  for (size_t i = 0; i < meshes.size(); i++) {
    auto &mesh = meshes[i];
    glBindVertexArray(vaos[i]);

    glBindBuffer(GL_ARRAY_BUFFER, vbos[i]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh.vertices.size(),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebos[i]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data(),
                 GL_STATIC_DRAW);

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
}

void destroy_buffers(std::vector<GLuint> &vaos, std::vector<GLuint> &vbos,
                     std::vector<GLuint> &ebos) {
  glDeleteVertexArrays(static_cast<GLsizei>(vaos.size()), vaos.data());
  glDeleteBuffers(static_cast<GLsizei>(vbos.size()), vbos.data());
  glDeleteBuffers(static_cast<GLsizei>(ebos.size()), ebos.data());
}

GLuint create_shader_program(const char *vertex_shader_text,
                             const char *fragment_shader_text) {
  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_text, nullptr);
  glCompileShader(vertex_shader);

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_text, nullptr);
  glCompileShader(fragment_shader);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

void destroy_shader_program(GLuint program) { glDeleteProgram(program); }

void set_shader_uniform_mat4(GLuint program, const char *name,
                             const glm::mat4 &matrix) {
  GLuint location = glGetUniformLocation(program, name);
  glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

template<class T>
void set_shader_uniform(GLuint program, const char* name, const T& value);

Mesh get_icosphere_primitive(int subdivide_level) {
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
  Mesh mesh{};
  mesh.vertices = std::move(vertices);
  mesh.indices = std::move(indices);
  return mesh;
}

Mesh get_simplex_disk_primitive(int subdivide_level) {
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
  Mesh mesh{};
  mesh.vertices = std::move(vertices);
  mesh.indices = std::move(indices);
  return mesh;
}

std::vector<glm::dvec2> get_circle_primitive(int subdivide_level) {
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
  // // convert to float and construct Mesh
  // std::vector<Vertex> vertices{};
  // vertices.reserve(initial_vertices.size());
  // for (auto &v : initial_vertices) {
  //   Vertex vertex{};
  //   vertex.pos =
  //       glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), 0.0f);
  //   vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  //   vertex.texcoord = glm::vec2((v.x + 1.0f) * 0.5f, (v.y + 1.0f) * 0.5f);
  //   vertices.push_back(vertex);
  // }
  // std::vector<uint32_t> indices{};
  // for (size_t i = 0; i < initial_vertices.size(); i++) {
  //   indices.push_back(static_cast<uint32_t>(i));
  //   indices.push_back(static_cast<uint32_t>((i + 1) %
  //   initial_vertices.size())); indices.push_back(
  //       static_cast<uint32_t>(initial_vertices.size())); // center vertex
  // }
  // Vertex center_vertex{};
  // center_vertex.pos = glm::vec3(0.0f, 0.0f, 0.0f);
  // center_vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  // center_vertex.texcoord = glm::vec2(0.5f, 0.5f);
  // vertices.push_back(center_vertex);
  // Mesh mesh{};
  // mesh.vertices = std::move(vertices);
  // mesh.indices = std::move(indices);
  // return mesh;
  return initial_vertices;
}

struct ScopeGuard {
  ScopeGuard(std::function<void()> exit_func) : m_exit_func(exit_func) {}
  ~ScopeGuard() { m_exit_func(); }
  std::function<void()> m_exit_func{};
};

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define GUARD_EXIT(func_body)                                                  \
  ScopeGuard CONCAT(scope_guard_, __LINE__)([&]() func_body)

int main() {
  // auto meshes = load_obj_file();
  std::vector<Mesh> meshes{};
  // meshes.push_back(get_circle_primitive(5));
  meshes.push_back(get_icosphere_primitive(3));
  // meshes.push_back(get_simplex_disk_primitive(4));

  const int height = 600;
  const int width = 800;

  auto window = init_window(width, height);
  GUARD_EXIT({
    fmt::println("destroy_window now!");
    destroy_window(window);
    window = nullptr;
  });

  float scale = 1.0f;
  float scale_factor = 3.0f;
  float inv_scale_factor = 1.0f / scale_factor;
  key_callbacks.emplace_back(
      [&](GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
          if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
          } else if (key == GLFW_KEY_UP) {
            scale *= scale_factor;
          } else if (key == GLFW_KEY_DOWN) {
            scale *= inv_scale_factor;
          }
        }
      });

  std::vector<GLuint> vaos, vbos, ebos;
  create_buffers(meshes, vaos, vbos, ebos);
  GUARD_EXIT({
    fmt::println("destroy_buffers now!");
    destroy_buffers(vaos, vbos, ebos);
  });

  auto shader_program =
      create_shader_program(vertex_shader_text, fragment_shader_text);
  GUARD_EXIT({
    fmt::println("destroy_shader_program now!");
    destroy_shader_program(shader_program);
    shader_program = 0;
  });

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glfwSetTime(0.0);

  double start = glfwGetTime();
  // one rotation every 10 seconds
  double angular_velocity = 2 * glm::pi<double>() / 10.0;

  const double FIX_DELTA_TIME = 1.0 / 60.0;

  glEnable(GL_DEPTH_TEST);

  glm::mat4 rotation = glm::mat4(1.0f);

  while (!glfwWindowShouldClose(window)) {
    // render loop

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double current = glfwGetTime();
    double delta_time = std::min(current - start, FIX_DELTA_TIME);
    start = current;

    glm::mat4 model = glm::mat4(
        1.0f); // make sure to initialize matrix to identity matrix first
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    model = glm::scale(glm::identity<glm::mat4>(), glm::vec3{scale});
    // rotation =
    //     glm::rotate(rotation, static_cast<float>(delta_time *
    //     angular_velocity),
    //                 glm::vec3(1.0f, 1.0f, 1.0f));
    model = model * rotation;
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

    projection = glm::perspective(glm::radians(45.0f),
                                  (float)width / (float)height, 0.1f, 100.0f);

    glUseProgram(shader_program);
    set_shader_uniform_mat4(shader_program, "model", model);
    set_shader_uniform_mat4(shader_program, "view", view);
    set_shader_uniform_mat4(shader_program, "projection", projection);

    for (size_t i = 0; i < meshes.size(); i++) {
      glBindVertexArray(vaos[i]);
      glDrawElements(GL_TRIANGLES,
                     static_cast<GLsizei>(meshes[i].indices.size()),
                     GL_UNSIGNED_INT, 0);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}