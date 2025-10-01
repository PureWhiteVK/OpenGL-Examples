#include "raii_helper.h"
#include "shader.h"
#include "window.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <glm/ext.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <memory>
#include <thread>
#include <uv.h>

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <nfd.h>
#include <nfd.hpp>
#include <nfd_glfw3.h>

using namespace gl;

namespace fs = std::filesystem;
namespace ch = std::chrono;

// default shader text
static const char *vertex_shader_text = R"(\
#version 330 core
// one giant triangle that covers the screen
const vec2 gridPlane[3] = vec2[3](
  vec2(-1.0,-1.0),
  vec2( 3.0,-1.0),
  vec2(-1.0, 3.0)
);

void main() {
  vec2 p = gridPlane[gl_VertexID];
  gl_Position = vec4(p,0.0, 1.0); // using directly the clipped coordinates
}
)";

static const char *fragment_shader_text = R"(\
#version 330 core
out vec4 FragColor;
uniform vec2 framebuffer_size;
void main()
{
  vec2 coord = gl_FragCoord.xy / framebuffer_size;
  FragColor = vec4(coord,0.0,1.0);
}
)";

class FileWatcher {
public:
  using FileChangeCallback = std::function<void(const fs::path &)>;

public:
  uv_fs_event_t m_fs_event_handle{};
  uv_timer_t m_debounce_timer{};
  uv_loop_t m_monitor_loop{};
  uv_async_t m_stop_signal{};
  uv_async_t m_change_target_signal{};

  bool m_debounce_activated{false};
  uint64_t m_debounce_timeout{400};
  // after content updating, send a message to main queue
  fs::path m_target_file{};
  FileChangeCallback m_on_file_change{};

public:
  static bool check_err(int err) {
    if (err) {
      fmt::println("libuv error: {}", uv_strerror(err));
      return true;
    }
    return false;
  }

  static void on_debounce_timer(uv_timer_t *handle) {
    auto self = reinterpret_cast<FileWatcher *>(uv_loop_get_data(handle->loop));

    int err{};
    self->m_debounce_activated = false;
    // fmt::println("on debounce timer");
    self->m_on_file_change(self->m_target_file);
  }

  static void on_fs_event(uv_fs_event_t *handle, const char *filename,
                          int events, int status) {
    auto self = reinterpret_cast<FileWatcher *>(uv_loop_get_data(handle->loop));
    fmt::println("[{}] receive fs event signal {} {} {}",
                 ch::duration_cast<ch::milliseconds>(
                     ch::high_resolution_clock::now().time_since_epoch())
                     .count(),
                 filename, events, status);
    int err{};
    if (events == UV_RENAME) {
      self->set_target_file(self->m_target_file);
      return;
    }
    if (self->m_debounce_activated) {
      fmt::println("reset debounce timer");
      err = uv_timer_stop(&self->m_debounce_timer);
      check_err(err);
    } else {
      fmt::println("start debounce timer");
      self->m_debounce_activated = true;
    }
    err = uv_timer_start(&self->m_debounce_timer, self->on_debounce_timer,
                         self->m_debounce_timeout, 0);
    check_err(err);
  }

  static void on_stop(uv_async_t *handle) {
    auto self = reinterpret_cast<FileWatcher *>(uv_loop_get_data(handle->loop));

    int err{};
    fmt::println("receive stop signal, stop monitor loop");
    fmt::println("stop signal");
    uv_close(reinterpret_cast<uv_handle_t *>(&self->m_stop_signal), nullptr);
    if (!uv_is_closing(
            reinterpret_cast<uv_handle_t *>(&self->m_debounce_timer))) {
      fmt::println("stop debounce timer");
      err = uv_timer_stop(&self->m_debounce_timer);
      check_err(err);
    }
    if (!uv_is_closing(
            reinterpret_cast<uv_handle_t *>(&self->m_fs_event_handle))) {
      fmt::println("stop monitoring fs event");
      err = uv_fs_event_stop(&self->m_fs_event_handle);
      check_err(err);
    }
    if (!uv_is_closing(
            reinterpret_cast<uv_handle_t *>(&self->m_change_target_signal))) {
      fmt::println("stop chaneg target signal");
      uv_close(reinterpret_cast<uv_handle_t *>(&self->m_change_target_signal),
               nullptr);
    }
    fmt::println("stop monitor loop");
    uv_stop(handle->loop);
  }

  static void on_change_target(uv_async_t *handle) {
    auto self = reinterpret_cast<FileWatcher *>(uv_loop_get_data(handle->loop));

    int err{};
    fmt::println("stop previous monitoring fs event");
    err = uv_fs_event_stop(&self->m_fs_event_handle);
    check_err(err);

    fmt::println("stop previous debounce timer");
    err = uv_timer_stop(&self->m_debounce_timer);
    self->m_debounce_activated = false;
    check_err(err);

    fmt::println("start new monitoring fs event");
    err = uv_fs_event_start(&self->m_fs_event_handle, &self->on_fs_event,
                            self->m_target_file.c_str(), 0);
    check_err(err);

    // manually trigger file change event here
    self->m_on_file_change(self->m_target_file);
  }

public:
  FileWatcher(FileChangeCallback on_file_change)
      : m_on_file_change(on_file_change) {}

  ~FileWatcher() { uv_loop_close(&m_monitor_loop); }

  void stop() {
    fmt::println("send stop signal to monitor loop");
    uv_async_send(&m_stop_signal);
  }

  bool init() {
    int err{};
    fmt::println("init monitor loop");
    err = uv_loop_init(&m_monitor_loop);
    if (check_err(err)) {
      return false;
    }
    uv_loop_set_data(&m_monitor_loop, this);
    fmt::println("init fs event");
    err = uv_fs_event_init(&m_monitor_loop, &m_fs_event_handle);
    if (check_err(err)) {
      return false;
    }

    fmt::println("init timer");
    err = uv_timer_init(&m_monitor_loop, &m_debounce_timer);
    if (check_err(err)) {
      return false;
    }

    fmt::println("init async stop signal");
    err = uv_async_init(&m_monitor_loop, &m_stop_signal, &on_stop);
    if (check_err(err)) {
      return false;
    }

    fmt::println("init target rename signal");
    err = uv_async_init(&m_monitor_loop, &m_change_target_signal,
                        &on_change_target);
    if (check_err(err)) {
      return false;
    }
    return true;
  }

  void set_target_file(const fs::path &new_p) {
    if (new_p.empty() || !fs::is_regular_file(new_p)) {
      fmt::println("file [{}] is invalid", new_p.c_str());
      return;
    }
    int err{};
    m_target_file = new_p;
    fmt::println("set target file to {}", new_p.filename().c_str());
    err = uv_async_send(&m_change_target_signal);
    // trigger change immedietly here
    check_err(err);
  }

  int run() {
    int err{};
    fmt::println("start monitor loop");
    err = uv_run(&m_monitor_loop, UV_RUN_DEFAULT);
    fmt::println("monitor loop run (1st) returned with {}", err);
    // handle unfinished event here
    err = uv_run(&m_monitor_loop, UV_RUN_DEFAULT);
    fmt::println("monitor loop run (2nd) returned with {}", err);
    return err;
  }
};

int main() {
  const int height = 600;
  const int width = 600;
  Window window{width, height, "Shader Toy"};

  window.add_render_callback([&](Window &window) {
    auto &io = ImGui::GetIO();
    auto *w = window.get_handle();
    bool update = false;
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(w, GLFW_TRUE);
      return;
    }
  });

  Shader shader{vertex_shader_text, fragment_shader_text};
  fs::path target_file = {};
  uv_loop_t *loop = uv_default_loop();
  uv_async_t shader_file_change_signal{};
  using AsyncCallback = std::function<void(uv_async_t *)>;
  AsyncCallback shader_file_change_callback = [&](uv_async_t *) {
    fmt::println("loading shader file [{}]", target_file.filename().c_str());
    std::ifstream file{target_file, std::ifstream::in};
    if (!file.is_open()) {
      fmt::println("failed to open file [{}]", target_file.filename().c_str());
      return;
    }
    static std::array<char, 1024> buffer{};
    std::string content{};
    while (!file.eof()) {
      file.read(buffer.data(), buffer.size());
      content += buffer.data();
    }
    Shader new_shader{vertex_shader_text, content.c_str()};
    if (!new_shader.is_valid()) {
      fmt::println("failed to create new shader, use previous one");
    } else {
      fmt::println("update shader");
      shader = std::move(new_shader);
    }
  };
  shader_file_change_signal.data = &shader_file_change_callback;
  uv_async_init(loop, &shader_file_change_signal, [](uv_async_t *handle) {
    AsyncCallback *callback = reinterpret_cast<AsyncCallback *>(handle->data);
    (*callback)(handle);
  });

  FileWatcher watcher{
      [&shader_file_change_signal, &target_file](const fs::path &p) {
        // here the on_file_change callback is run on watcher thread,
        // we have to send another signal to make sure shader load action is
        // executed on main thread
        target_file = p;
        uv_async_send(&shader_file_change_signal);
      }};

  std::thread watcher_thread{[&watcher]() {
    watcher.init();
    watcher.run();
  }};

  window.add_render_callback([&watcher, &target_file](Window &window) {
    auto &io = ImGui::GetIO();
    // static bool show_demo_window{false};
    // ImGui::ShowDemoWindow(&show_demo_window);
    {
      ImGui::Begin("Control Panel");
      // ImGui::Checkbox("Demo Window", &show_demo_window);
      static nfdu8char_t *file_path{};
      if (ImGui::Button("Fragment Shader")) {
        static nfdfilteritem_t filter{"Fragment Shader", "frag"};
        static nfdwindowhandle_t native_window{};
        if (!NFD_GetNativeWindowFromGLFWWindow(window.get_handle(),
                                               &native_window)) {
          const char *err_msg = NFD_GetError();
          if (err_msg) {
            fmt::println("failed to get native window: {}", err_msg);
          }
        }
        nfdresult_t res = NFD::OpenDialog(
            file_path, &filter, 1, "/Users/xiao/Code/Cpp/OpenGL-Examples",
            native_window);
        if (res == NFD_OKAY) {
          watcher.set_target_file(file_path);
        }
      }
      ImGui::Text("Current Shader File: %s", file_path ? file_path : "(null)");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }
    ImGui::Render();
  });

  GLuint dummy_vao{};
  glGenVertexArrays(1, &dummy_vao);
  DEFER({ glDeleteVertexArrays(1, &dummy_vao); });

  window.add_render_callback([loop, &shader, dummy_vao](Window &window) {
    // handle change here
    uv_run(loop, UV_RUN_NOWAIT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!shader.is_valid()) {
      return;
    }
    auto [w, h] = window.get_framebuffer_size();
    shader.use();
    shader.set_uniform("framebuffer_size",
                       glm::vec2{static_cast<float>(w), static_cast<float>(h)});
    glBindVertexArray(dummy_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  });

  window.run();
  watcher.stop();
  watcher_thread.join();
  return 0;
}