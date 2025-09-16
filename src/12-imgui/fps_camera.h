#ifndef FPS_CAMERA_H
#define FPS_CAMERA_H

#include "camera.h"
#include "window.h"
#include <glm/glm.hpp>

class FpsCamera: public Camera {
public:
  FpsCamera(const glm::vec3 world_up = glm::vec3{0.0f, 1.0f, 0.0f});
  ~FpsCamera() override = default;
  void update() override;
  void draw_gui() override;
  void draw_indicator() override {};

private:
  void window_callback(Window &window) override;

private:
  const glm::vec3 m_world_up{};
  float m_pitch{0.0f};
  float m_yaw{0.0f};
  float m_move_speed{1.0f};
  float m_view_sensitivity{0.1f};
};

#endif // FPS_CAMERA_H