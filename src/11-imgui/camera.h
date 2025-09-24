#ifndef CAMERA_H
#define CAMERA_H

#include "window.h"
#include <glm/glm.hpp>

class Camera {
public:
  Camera() = default;
  virtual ~Camera() = default;
  const glm::mat4 &get_view_matrix() const { return m_view_mat; }
  virtual void update() = 0;
  void install_callback(Window &window) {
    m_callback_handle = window.add_render_callback(
        [this](Window &window) { this->window_callback(window); });
  };
  void remove_callback(Window &window) {
    window.remove_render_callback(m_callback_handle);
  }
  virtual void draw_gui() = 0;
  virtual void draw_indicator() = 0;

protected:
  virtual void window_callback(Window &window) = 0;

protected:
  CallbackHandle m_callback_handle{};
  glm::mat4 m_view_mat{1.0f};
  glm::vec3 m_pos{0.0f, 0.0f, 0.0f};
  glm::vec3 m_forward{};
  glm::vec3 m_right{};
  glm::vec3 m_up{};
};

#endif // CAMERA_H