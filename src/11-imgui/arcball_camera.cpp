#include "arcball_camera.h"
#include "primitive.h"
#include "shader.h"
#include "window.h"

#include <cassert>
#include <fmt/format.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glm/ext.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <imgui.h>
#include <memory>

static const char *vertex_shader_text = R"(\
#version 330 core
layout (location = 0) in vec3 aPos;

out float diameter;

void main()
{
	// gl_Position = projection * view * model * vec4(aPos, 1.0);
  gl_Position = vec4(0.0,0.0,0.0,1.0);
  // diameter for point
  gl_PointSize = 30.0;
  diameter = gl_PointSize;
}
)";

static const char *fragment_shader_text = R"(\
#version 330 core
out vec4 FragColor;

in float diameter;

void main()
{
  float radius = diameter * 0.5;
  float thickness = 3.0;
  // 将 gl_PointCoord 映射到 [-1,1] 坐标
  vec2 uv = gl_PointCoord * 2.0 - 1.0;
  float dist = length(uv) * radius;

  // 圆环裁剪
  if (abs(dist - radius) > thickness * 0.5)
      discard;

  // 计算角度
  float angle = atan(uv.y, uv.x); // [-pi, pi]
  if (angle < 0.0) angle += 2.0 * 3.1415926; // 转成 [0,2pi]

  // 8 分支，每个扇区 π/4
  int sector = int(floor(angle / (3.1415926/4.0)));

  // 奇偶扇区蓝白交替
  if (sector % 2 == 0)
      FragColor = vec4(1.0,0.0,0.0,1.0);
  else
      FragColor = vec4(1.0,1.0,1.0,1.0);
}
)";

ArcBallCamera::ArcBallCamera(const glm::vec3 world_up) : m_world_up(world_up) {
  using namespace gl;
  update();
  glGenVertexArrays(1, &m_vao);
  m_mesh = get_icosphere_primitive(2);
  m_mesh->create_buffer();
  m_mesh_shader = Shader(vertex_shader_text, fragment_shader_text);
  assert(m_mesh_shader.get_program_id() != 0);
  float range[2];
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  fmt::println("Point Size range: {},{}", range[0], range[1]);
  glEnable(GL_PROGRAM_POINT_SIZE);
}

ArcBallCamera::~ArcBallCamera() { gl::glDeleteVertexArrays(1, &m_vao); }

void ArcBallCamera::update() {
  float theta = glm::radians(m_yaw);
  float phi = glm::radians(m_pitch);
  // camera will look at -z direction
  m_forward = glm::vec3{cos(theta) * cos(phi), sin(phi), sin(theta) * cos(phi)};
  m_pos = m_view_center + m_radius * m_forward;
  m_right = glm::normalize(glm::cross(m_forward, m_world_up));
  m_up = glm::normalize(glm::cross(m_right, m_forward));
  m_view_mat = glm::lookAt(m_pos, m_view_center, m_up);
}

void ArcBallCamera::window_callback(Window &window) {
  auto &io = ImGui::GetIO();
  auto *w = window.get_handle();
  bool update_flag = false;
  if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
    return;
  }
  m_is_moving = io.KeyShift;
  // handle mouse event here
  if (io.MouseDown[ImGuiMouseButton_Left]) {
    if (io.KeyShift) {
      // move view center along current right left or up or down
      m_view_center += m_right * io.MouseDelta.x * m_move_speed;
      m_view_center += m_up * io.MouseDelta.y * m_move_speed;
    } else {
      m_yaw += io.MouseDelta.x * m_view_sensitivity;
      if(m_yaw > 179.0f) {
        m_yaw -= 360.0f;
      } else if(m_yaw < -179.0f) {
        m_yaw += 360.0f;
      }
      // inverse y
      m_pitch += io.MouseDelta.y * m_view_sensitivity;
      m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    }
  }
  m_radius += -1.0 * io.MouseWheel * m_scale_sensitivity;
  m_radius = glm::clamp(m_radius, 0.1f, 20.0f);
  update();
}

void ArcBallCamera::draw_gui() {
  ImGui::Begin("ArcBall Camera");
  ImGui::SliderFloat("View Sensitivity", &m_view_sensitivity, 0.0f, 1.0f);
  ImGui::SliderFloat("Scale Sensitivity", &m_scale_sensitivity, 0.0f, 1.0f);
  ImGui::SliderFloat("Move Speed", &m_move_speed, 0.0f, 0.01f);
  ImGui::SliderFloat("Radius", &m_radius, 0.1f, 20.0f);
  ImGui::SliderFloat3("View Center", glm::value_ptr(m_view_center), -20, 20);
  ImGui::SliderFloat("Pitch", &m_pitch, -89.0f, 89.0f);
  ImGui::SliderFloat("Yaw", &m_yaw, -180.0f, 180.0f);
  // matrix is columa major here
  ImGui::Text(
      "[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f "
      "%.3f]\n[%.3f %.3f %.3f %.3f]",
      m_view_mat[0][0], m_view_mat[1][0], m_view_mat[2][0], m_view_mat[3][0],
      m_view_mat[0][1], m_view_mat[1][1], m_view_mat[2][1], m_view_mat[3][1],
      m_view_mat[0][2], m_view_mat[1][2], m_view_mat[2][2], m_view_mat[3][2],
      m_view_mat[0][3], m_view_mat[1][3], m_view_mat[2][3], m_view_mat[3][3]);
  ImGui::End();
}

void ArcBallCamera::draw_indicator() {
  if (!m_is_moving) {
    return;
  }
  using namespace gl;
  // glm::mat4 model =
  //     glm::scale(glm::translate(glm::identity<glm::mat4>(), m_view_center),
  //                glm::vec3{0.1});
  m_mesh_shader.use();
  // m_mesh_shader.set_uniform("projection", proj_mat);
  // m_mesh_shader.set_uniform("view", m_view_mat);
  // m_mesh_shader.set_uniform("model", model);
  glDisable(GL_DEPTH_TEST);
  // m_mesh->draw();
  glBindVertexArray(m_vao);
  glDrawArrays(GL_POINTS, 0, 1);
  glEnable(GL_DEPTH_TEST);
}