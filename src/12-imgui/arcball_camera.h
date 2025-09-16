#ifndef ARCBALL_CAMERA_H
#define ARCBALL_CAMERA_H

#include "camera.h"
#include "mesh.h"
#include "window.h"
#include "shader.h"
#include <glbinding/gl/types.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <memory>

class ArcBallCamera : public Camera {
public:
  ArcBallCamera(const glm::vec3 world_up = glm::vec3{0.0f, 1.0f, 0.0f});
  ~ArcBallCamera() override;
  const glm::mat4 &get_view_matrix() const { return m_view_mat; }
  void update() override;
  void draw_gui() override;
  void draw_indicator() override;

protected:
  void window_callback(Window &window) override;

private:
  glm::vec3 m_view_center{0.0f, 0.0f, 0.0f};
  const glm::vec3 m_world_up{};
  float m_radius{5.0f};
  float m_pitch{0.0f};
  float m_yaw{0.0f};
  float m_move_speed{0.005f};
  float m_view_sensitivity{0.1f};
  float m_scale_sensitivity{0.1f};
  std::unique_ptr<Mesh> m_mesh{};
  gl::GLuint m_vao{};
  Shader m_mesh_shader;
  bool m_is_moving{false};
};

#endif // ARCBALL_CAMERA_H