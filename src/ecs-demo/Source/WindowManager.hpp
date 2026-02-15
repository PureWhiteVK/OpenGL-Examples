#pragma once

#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <bitset>
#include <string>

using namespace gl;

class WindowManager {
public:
  void Init(std::string const &windowTitle, unsigned int windowWidth,
            unsigned int windowHeight, unsigned int windowPositionX,
            unsigned int windowPositionY);

  void Update();

  void ProcessEvents();

  void Shutdown();

private:
  GLFWwindow *mWindow;

  std::bitset<8> mButtons;
};
