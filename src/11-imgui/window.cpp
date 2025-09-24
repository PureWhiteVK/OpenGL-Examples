#include "window.h"

#include <glbinding-aux/ContextInfo.h>
#include <glbinding-aux/debug.h>
#include <glbinding-aux/types_to_string.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

using namespace gl;

template <> struct fmt::formatter<GLenum> : fmt::ostream_formatter {};

CallbackHandle Window::add_render_callback(RenderCallback callback) {
  static int index = 0;
  CallbackHandle handle{index};
  index++;
  m_render_callbacks.emplace(handle, callback);
  return handle;
}

RenderCallback Window::remove_render_callback(CallbackHandle handle) {
  auto pos = m_render_callbacks.find(handle);
  if (pos == m_render_callbacks.end()) {
    return RenderCallback{};
  }
  RenderCallback callback = pos->second;
  m_render_callbacks.erase(pos);
  return callback;
}

Window::Window(int width, int height, const std::string &title)
    : m_width(width), m_height(height) {
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

  m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!m_window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetWindowUserPointer(m_window, this);
  glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width,
                                         int height) {
    auto self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    self->m_width = width;
    self->m_height = height;
    int display_w{}, display_h{};
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
  });

  glfwMakeContextCurrent(m_window);
  glbinding::initialize(glfwGetProcAddress);

  fmt::println("OpenGL Version: {}",
               glbinding::aux::ContextInfo::version().toString());

  // disable all debug message here
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                        GL_FALSE);
  // enable severity high
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        gl::GLenum::GL_DEBUG_SEVERITY_HIGH, 0, nullptr,
                        GL_TRUE);
  // output log
  glDebugMessageCallback(
      [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
         const GLchar *message, const void *userParam) {
        fmt::println(
            "[OpenGL Debug] source = {}, type = {}, id = {}, severity = {}, "
            "message = {}",
            source, type, id, severity, message);
      },
      nullptr);

  glbinding::aux::enableGetErrorCallback();

  // Enable vsync
  glfwSwapInterval(1);

  // initialize ImGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;
  io.ConfigDpiScaleFonts = true;
  io.ConfigDpiScaleViewports = true;

  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init();
}

Window::~Window() {
  fmt::println("cleanup imgui");
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  fmt::println("destroy window");
  glfwDestroyWindow(m_window);
  glfwTerminate();
}

void Window::render_loop() {
  auto &imgui_io = ImGui::GetIO();
  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0) {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Rendering (or we can say, on_render_frame)
    for (auto &[key, render_callback] : m_render_callbacks) {
      render_callback(*this);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we
    // save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call
    //  glfwMakeContextCurrent(window) directly)

    if (imgui_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow *backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }
    glfwSwapBuffers(m_window);
  }
}