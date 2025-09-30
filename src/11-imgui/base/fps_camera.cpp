#include "fps_camera.h"
#include "window.h"

#include <glm/ext.hpp>
#include <imgui.h>

FpsCamera::FpsCamera(const glm::vec3 world_up) : m_world_up(world_up) {
  update();
}

void FpsCamera::update() {
  float theta = glm::radians(m_yaw);
  float phi = glm::radians(m_pitch);
  // camera will look at -z direction
  m_forward =
      glm::vec3{cos(theta) * cos(phi), sin(phi), -1.0f * sin(theta * cos(phi))};
  m_right = glm::normalize(glm::cross(m_forward, m_world_up));
  m_up = glm::normalize(glm::cross(m_right, m_forward));
  m_view_mat = glm::lookAt(m_pos, m_pos + m_forward, m_up);
}

void FpsCamera::window_callback(Window &window) {
  auto &io = ImGui::GetIO();
  auto *w = window.get_handle();
  bool update_flag = false;
  if (!io.WantCaptureKeyboard) {
    // handle keyboard event here
    glm::vec3 move_dir{0.0f};
    // move forward
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) {
      move_dir += m_forward;
      update_flag = true;
    }
    // move left
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) {
      move_dir -= m_right;
      update_flag = true;
    }
    // move backward
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) {
      move_dir -= m_forward;
      update_flag = true;
    }
    // move right
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) {
      move_dir += m_right;
      update_flag = true;
    }
    // move up
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) {
      move_dir += m_up;
      update_flag = true;
    }
    // move down
    if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      move_dir -= m_up;
      update_flag = true;
    }
    if (update_flag) {
      float move_dir_length = glm::length(move_dir);
      m_pos += (move_dir_length > 0.1 ? move_dir / move_dir_length
                                      : glm::vec3{0.0f}) *
               m_move_speed * io.DeltaTime;
    }
  }
  if (!io.WantCaptureMouse) {
    // handle mouse event here
    m_yaw += io.MouseDelta.x * m_view_sensitivity * -1.0f;
    // inverse y
    m_pitch += io.MouseDelta.y * m_view_sensitivity * -1.0f;
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    update_flag = true;
  }
  if (update_flag) {
    update();
  }
}

void FpsCamera::draw_gui() {
  ImGui::Begin("FPS Camera");
  ImGui::SliderFloat("View Sensitivity", &m_view_sensitivity, 0.0f, 1.0f);
  ImGui::SliderFloat("Move Speed", &m_move_speed, 0.0f, 10.0f);
  ImGui::SliderFloat3("Position", glm::value_ptr(m_pos), -20, 20);
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