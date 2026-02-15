#include "WindowManager.hpp"

#include "Core/Coordinator.hpp"
#include <bitset>
#include <iostream>


extern Coordinator gCoordinator;


// TODO: Return error to caller
void WindowManager::Init(
	std::string const& windowTitle, unsigned int windowWidth, unsigned int windowHeight, unsigned int windowPositionX,
	unsigned int windowPositionY)
{
	glfwInit();


  // high dpi support
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#endif
  // Request an OpenGL 4.2 core profile context
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
	
	mWindow = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), NULL, NULL);

	// Create OpenGL Context
	glfwMakeContextCurrent(mWindow);
	glbinding::initialize(glfwGetProcAddress);
	glfwSwapInterval(2);

	// Configure OpenGL
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
}


void WindowManager::Update()
{
	glfwSwapBuffers(mWindow);
}


void WindowManager::Shutdown()
{
	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

void WindowManager::ProcessEvents()
{
	glfwPollEvents();

	bool buttonStateChanged = true;

	if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE))
	{
		gCoordinator.SendEvent(Events::Window::QUIT);
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_W))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::W));
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_A))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::A));
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_S))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::S));
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_D))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::D));
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_Q))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::Q));
	}
	else if (glfwGetKey(mWindow, GLFW_KEY_E))
	{
		mButtons.set(static_cast<std::size_t>(InputButtons::E));
	}
	else
	{
		buttonStateChanged = false;
	}

	if (buttonStateChanged)
	{
		Event event(Events::Window::INPUT);
		event.SetParam(Events::Window::Input::INPUT, mButtons);
		gCoordinator.SendEvent(event);
	}
}
