#include "shader.h"
#include <fmt/base.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/types.h>
#include <glm/ext.hpp>

#include "raii_helper.h"
using namespace gl;

static const char *vertex_shader_text = R"(\
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

static const char *fragment_shader_text = R"(\
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
// uniform sampler2D texture1;
// uniform sampler2D texture2;
// world grid scale, use this scale factor to make sure the size is a constant
// show grid at a fixed size ?
uniform float scale;

void main()
{
  FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0);
}
)";

Shader::Shader(const char *vertex_shader_source,
               const char *fragment_shader_source) {
  GLint status{};
  GLsizei length{};
  std::string log_buffer{};

  auto check_shader_compile_status = [&](GLuint shader) {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
      log_buffer.resize(length);
      glGetShaderInfoLog(shader, log_buffer.size(), nullptr, log_buffer.data());
      fmt::println("ShaderInfoLog:\n{}", log_buffer.data());
    }
    return status;
  };

  auto check_program_link_status = [&](GLuint program) {
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
      log_buffer.resize(length);
      glGetProgramInfoLog(program, log_buffer.size(), nullptr,
                          log_buffer.data());
      fmt::println("ProgramInfoLog:\n{}", log_buffer.data());
    }
    return status;
  };

  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
  DEFER({ glDeleteShader(vertex_shader); });
  glCompileShader(vertex_shader);
  if (!check_shader_compile_status(vertex_shader)) {
    return;
  }

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
  DEFER({ glDeleteShader(fragment_shader); });
  glCompileShader(fragment_shader);
  if (!check_shader_compile_status(fragment_shader)) {
    return;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  if (!check_program_link_status(program)) {
    glDeleteProgram(program);
    return;
  }
  this->program = program;
}

Shader::~Shader() {
  if (is_valid()) {
    fmt::println("delete program: {}", program);
    glDeleteProgram(program);
  }
}

void Shader::use() { glUseProgram(program); }

void Shader::set_uniform(const char *name, const glm::mat4 &matrix) {
  GLuint location = glGetUniformLocation(program, name);
  glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::set_uniform(const char *name, const glm::vec3 &vector) {
  GLuint location = glGetUniformLocation(program, name);
  glUniform3fv(location, 1, glm::value_ptr(vector));
}

void Shader::set_uniform(const char *name, const glm::vec2 &vector) {
  GLuint location = glGetUniformLocation(program, name);
  glUniform2fv(location, 1, glm::value_ptr(vector));
}

void Shader::set_uniform(const char *name, const float f) {
  GLuint location = glGetUniformLocation(program, name);
  glUniform1f(location, f);
}

Shader get_test_shader() {
  return Shader{vertex_shader_text, fragment_shader_text};
}

// https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/
static const char *grid_vs = R"(\
#version 330 core

uniform mat4 inv_view_proj;

// one giant triangle that covers the screen
const vec2 gridPlane[3] = vec2[3](
  vec2(-1.0,-1.0),
  vec2( 3.0,-1.0),
  vec2(-1.0, 3.0)
);

out vec3 near_point;
out vec3 far_point;

void main() {
    vec2 p = gridPlane[gl_VertexID];
    // near point in ndc coordinate
    vec4 point = inv_view_proj * vec4(p,-1.0,1.0);
    near_point = point.xyz / point.w;
    // far point in ndc corrdinate
    point = inv_view_proj * vec4(p,1.0,1.0);
    far_point = point.xyz / point.w;
    gl_Position = vec4(p,0.0, 1.0); // using directly the clipped coordinates
}
)";

static const char *grid_fs = R"(\
#version 330 core

in vec3 near_point;
in vec3 far_point;

out vec4 out_color;

uniform mat4 proj_view;
uniform float near;
uniform float far;

uniform vec3 grid_color = vec3(0.1,0.1,0.1);
uniform float grid_size = 0.5;
uniform float grid_size_scale = 5;
uniform float grid_thickness1 = 1.5;
uniform float grid_thickness2 = 2.5;
uniform float max_depth = 0.5;

float compute_depth(vec3 pos) {
    vec4 clip_space_pos = proj_view * vec4(pos.xyz, 1.0);
    // re-arrange depth to 0~1
    return (clip_space_pos.z / clip_space_pos.w) * 0.5 + 0.5;
}

float compute_linear_depth01(float zndc) {
  // float k1 = (far + near) / (far - near);
  // float k2 = -2.0 * near * far / (far - near);
  // float z = k2 / (zndc - k1);
  // float z01 = (z - near) / (far-near);
  float z01 = (2.0 * near) / (near + far - (far - near) * zndc);
  return z01;
}

vec4 draw_grid(vec3 world_pos, float grid_size, float grid_thickness, bool draw_axis){
  vec2 grid_coord = world_pos.xz / grid_size;
  // pixel size in world space
  vec2 pixel_size = fwidth(grid_coord);
  // distance to grid line in world space
  vec2 grid_dist_world = abs(fract(grid_coord - 0.5) - 0.5);
  // distance to grid line in screen space
  vec2 grid_dist_pixel = grid_dist_world / pixel_size;
  // select min distance as grid line
  float grid_dist = min(grid_dist_pixel.x,grid_dist_pixel.y);
  float draw_line = smoothstep(grid_thickness,0.0,grid_dist);

  vec3 curr_color = grid_color;
  vec2 grid_index = floor(grid_coord);
  vec2 min_axis_line_dist = min(pixel_size,grid_size);

  if(!draw_axis) {
    return vec4(curr_color,draw_line);
  }

  if(abs(world_pos.x) < min_axis_line_dist.x) {
    curr_color.xyz = vec3(1.0,0.0,0.0);
  }
  if(abs(world_pos.z) < min_axis_line_dist.y) {
    curr_color.xyz = vec3(0.0,0.0,1.0);
  }
  return vec4(curr_color,draw_line);
}

void main() {
    float t = -near_point.y / (far_point.y - near_point.y);
    vec3 world_pos = near_point + t * (far_point - near_point);
    float depth_clip = compute_depth(world_pos);
    gl_FragDepth = depth_clip;
    float linear_depth_01 = compute_linear_depth01(depth_clip * 2.0 - 1.0);
    float depth_fading = (1.0 / max_depth) * max(0.0, max_depth - linear_depth_01);
    float draw_plane = float(t>0.0);
    
    vec4 grid1 = draw_grid(world_pos,grid_size,grid_thickness1,true);
    vec4 grid2 = draw_grid(world_pos,grid_size * grid_size_scale,grid_thickness2,true);

    vec4 color = 0.5 * (grid1 + grid2);
    
    out_color = vec4(color.xyz,draw_plane * depth_fading * color.w);
}
)";

Shader get_grid_shader() { return Shader{grid_vs, grid_fs}; }