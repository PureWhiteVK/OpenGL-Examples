#ifndef WINDOW_H
#define WINDOW_H

#include <functional>
#include <map>
#include <stdint.h>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// using KeyCallback = std::function<void(GLFWwindow *, int, int, int, int)>;
// using CallbackHandle = int;

class Window;

using RenderCallback = std::function<void(Window &)>;
using CallbackHandle = int;

class Window {
public:
  Window(int width, int height, const std::string &title);
  ~Window();

  GLFWwindow *get_handle() { return m_window; }

  std::tuple<int, int> get_window_size() { return {m_width, m_height}; }
  std::tuple<int, int> get_framebuffer_size() { return {m_display_width,m_display_height}; }

  void run();

  CallbackHandle add_render_callback(RenderCallback callback);
  RenderCallback remove_render_callback(CallbackHandle handle);

private:
  GLFWwindow *m_window{nullptr};
  int m_height{};
  int m_width{};
  int m_display_height{};
  int m_display_width{};
  std::map<CallbackHandle, RenderCallback> m_render_callbacks{};
};

#endif // WINDOW_H