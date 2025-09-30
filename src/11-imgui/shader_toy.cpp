#include "arcball_camera.h"
#include "drawable.h"
#include "fps_camera.h"
#include "primitive.h"
#include "raii_helper.h"
#include "shader.h"
#include "window.h"

#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glbinding/FunctionCall.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/types.h>
#include <glm/ext.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <memory>

using namespace gl;

class TransformComponent {
public:
  const glm::mat4 &get_transform() const { return m_trans; }
  const glm::mat4 &get_inv_transform() const { return m_inv_trans; }

  void draw_gui() {
    ImGui::Begin("Transform");
    bool scale_change =
        ImGui::SliderFloat3("Scale", glm::value_ptr(m_scale), 0.0f, 1.0f);
    bool translate_change = ImGui::SliderFloat3(
        "Translate", glm::value_ptr(m_translate), -20.0f, 20.0f);
    ImGui::End();

    // update transform here
    if (scale_change || translate_change) {
      m_trans = glm::translate(glm::scale(glm::identity<glm::mat4>(), m_scale),
                               m_translate);
      m_inv_trans = glm::inverse(m_trans);
    }
  }

private:
  glm::mat4 m_trans{1.0f};
  glm::mat4 m_inv_trans{1.0f};
  glm::vec3 m_translate{0.0f};
  glm::vec3 m_scale{1.0f};
};

static const char *vertex_shader_text = R"(\
#version 330 core

uniform vec3 point; 
uniform mat4 view;
uniform mat4 projection;


out float diameter;

void main()
{
	gl_Position = projection * view * vec4(point, 1.0);
  // gl_Position = vec4(0.0,0.0,0.0,1.0);
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

int main() {
  const int height = 600;
  const int width = 800;
  Window window{width, height, "OpenGL Geometry Primitives"};
  // auto meshes = load_obj_file();
  std::vector<std::unique_ptr<Drawable>> drawing_objs{};

  std::unique_ptr<Drawable> test_cube_wireframe =
      get_cube_wireframe_primitive();

  std::unique_ptr<Mesh> test_cube_triangle = std::make_unique<Mesh>();
  test_cube_triangle->vertices = {
      {{0.5, 1.05, 1.0}}, {{0.0, 1.55, 1.0}}, {{-0.5, 1.05, 1.0}}};
  test_cube_triangle->indices = {0, 1, 2};
  test_cube_triangle->create_buffer();
  drawing_objs.emplace_back(get_polygon_cone_primitive(4));
  drawing_objs.emplace_back(get_cylinder_primitive(3));
  // drawing_objs.emplace_back(get_plane_primitive());
  drawing_objs.emplace_back(get_icosphere_primitive(3));
  // drawing_objs.emplace_back((get_simplex_disk_primitive(2)));
  // drawing_objs.emplace_back(get_plane_primitive());
  drawing_objs.emplace_back(get_cube_wireframe_primitive());
  drawing_objs.emplace_back(get_cube_primitive());
  std::shared_ptr<ArcBallCamera> arcball_camera =
      std::make_shared<ArcBallCamera>();
  std::shared_ptr<FpsCamera> fps_camera = std::make_shared<FpsCamera>();
  std::shared_ptr<ArcBallCamera> camera = arcball_camera;

  camera->install_callback(window);

  float scale = 1.0f;
  float scale_factor = 3.0f;
  float inv_scale_factor = 1.0f / scale_factor;

  test_cube_triangle->create_buffer();
  test_cube_wireframe->create_buffer();
  for (auto &m : drawing_objs) {
    m->create_buffer();
  }
  GUARD_EXIT({
    for (auto &m : drawing_objs) {
      m->destroy_buffer();
    }
    test_cube_wireframe->destroy_buffer();
    test_cube_triangle->destroy_buffer();
  });

  auto shader_program = get_test_shader();
  if (shader_program.get_program_id() == 0) {
    fmt::println("failed to create shader, quit!");
    return -1;
  }

  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  // final_color = src_alpha * src_color + dst_color * dst_factor
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glfwSetTime(0.0);

  double start = glfwGetTime();
  // one rotation every 10 seconds
  double angular_velocity = 2 * glm::pi<double>() / 10.0;

  const double FIX_DELTA_TIME = 1.0 / 60.0;

  glm::mat4 rotation = glm::mat4(1.0f);

  bool show_demo_window = true;
  bool show_another_window = false;
  bool draw_grid = true;
  glm::vec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

  GLuint dummy_vao{};
  glGenVertexArrays(1, &dummy_vao);
  GUARD_EXIT({ glDeleteVertexArrays(1, &dummy_vao); });
  auto grid_shader = get_grid_shader();
  if (grid_shader.get_program_id() == 0) {
    fmt::println("failed to create shader, quit!");
    return -1;
  }

  std::vector<glm::vec3> model_pos{};
  for (int i = 0; i < drawing_objs.size(); i++) {
    model_pos.emplace_back(glm::vec3{0.0f, 0.0f, 0.0f});
  }

  // glfwSetInputMode(window.get_handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  window.add_render_callback([&](Window &window) {
    auto &io = ImGui::GetIO();
    auto *w = window.get_handle();
    bool update = false;
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(w, GLFW_TRUE);
      return;
    }
  });

  TransformComponent trans_component{};

  window.add_render_callback([&](Window &window) {
    auto &io = ImGui::GetIO();
    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
    // ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to create a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                     // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      // Edit bools storing our window open/close state
      ImGui::Checkbox("Demo Window", &show_demo_window);

      ImGui::Checkbox("Draw Grid", &draw_grid);

      for (int i = 0; i < model_pos.size(); i++) {
        std::string label = fmt::format("model position [{:02}]", i);
        ImGui::SliderFloat3(label.c_str(), glm::value_ptr(model_pos[i]), -20,
                            20);
      }

      if (ImGui::Button("Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    camera->draw_gui();

    trans_component.draw_gui();

    ImGui::Render();
  });

  GLuint test_vao{};
  glGenVertexArrays(1, &test_vao);
  GUARD_EXIT({ glDeleteVertexArrays(1, &test_vao); });

  Shader point_shader{vertex_shader_text, fragment_shader_text};

  float near = 0.01f;
  float far = 100.0f;

  window.add_render_callback([&](Window &window) {
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double current = glfwGetTime();
    double delta_time = std::min(current - start, FIX_DELTA_TIME);
    start = current;

    glm::mat4 projection = glm::mat4(1.0f);
    auto [display_w, display_h] = window.get_window_size();

    float dist = camera->m_radius;
    float ratio = static_cast<float>(display_w) / static_cast<float>(display_h);
    projection =
        glm::ortho(-dist * ratio, dist * ratio, -dist, dist, -100.0f, 100.0f);

    projection = glm::perspective(
        glm::radians(45.0f), (float)display_w / (float)display_h, near, far);
    const glm::mat4 &view = camera->get_view_matrix();
    shader_program.use();
    shader_program.set_uniform("view", view);
    shader_program.set_uniform("projection", projection);

    for (int i = 0; i < drawing_objs.size(); i++) {
      glm::mat4 model =
          glm::translate(glm::identity<glm::mat4>(), model_pos[i]);

      shader_program.set_uniform("model", model);
      drawing_objs[i]->draw();
    }

    glm::vec3 test_camera_pos = glm::vec3{3.0f};
    glm::mat4 test_view = glm::lookAt(test_camera_pos, glm::vec3{0.0f},
                                      glm::vec3{0.0f, 1.0f, 0.0f});

    // test_view = glm::identity<glm::mat4>();

    glm::mat4 inv_camera = glm::inverse(test_view) * glm::inverse(projection);

    // make sure camera will scale to 1
    float camera_model_scale = (far + near - 2.0 * near * far) / (far - near);

    shader_program.set_uniform(
        "model", inv_camera * glm::scale(glm::identity<glm::mat4>(),
                                         glm::vec3{camera_model_scale}));
    test_cube_wireframe->draw();
    test_cube_triangle->draw();

    // point_shader.use();
    // point_shader.set_uniform("view", view);
    // point_shader.set_uniform("projection", projection);
    // point_shader.set_uniform("point", test_camera_pos);
    // glBindVertexArray(test_vao);
    // glDrawArrays(GL_POINTS, 0, 1);

    if (draw_grid) {
      glm::mat4 proj_view = projection * view;
      glm::mat4 inv_view_proj = glm::inverse(proj_view);
      grid_shader.use();

      grid_shader.set_uniform("proj_view", proj_view);
      grid_shader.set_uniform("inv_view_proj", inv_view_proj);
      grid_shader.set_uniform("near", near);
      grid_shader.set_uniform("far", far);

      glBindVertexArray(dummy_vao);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    camera->draw_indicator();
  });

  

  // shader toy like fragment shader set up
  window.add_render_callback([&](Window& window){
    glClearColor(0, 0, 0, 1);

  });
  window.render_loop();
  return 0;
}