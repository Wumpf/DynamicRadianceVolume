#include "outputwindow.hpp"

#include "glhelper/texture2d.hpp"
#include "glhelper/screenalignedtriangle.hpp"

#include "utilities/logger.hpp"
#include "utilities/utils.hpp"

#include <iostream>
#include <cassert>
#include <memory>
#include <string>

std::unordered_map<GLFWwindow*, std::vector<OutputWindow::ResizeHandlerFunc>> OutputWindow::s_resizeHandlers;

static void ErrorCallbackGLFW(int error, const char* description)
{
	LOG_ERROR("GLFW error, code " + std::to_string(error) + " desc: \"" + description + "\"");
}

void WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	// Ignore invalid width/height (might be minimizing).
	if (width <= 0 || height <= 0)
		return;

	LOG_INFO("Resizing framebuffer to " << width << "x" << height);

	ei::IVec2 resolution;
	glfwGetFramebufferSize(window, &resolution.x, &resolution.y);

	auto& handlers = OutputWindow::s_resizeHandlers[window];
	for (auto& handler : handlers)
		handler(resolution.x, resolution.y);
}

OutputWindow::OutputWindow() :
	m_displayHDR("displayHDR")
{
	if (!glfwInit())
		throw std::exception("GLFW init failed");

	glfwSetErrorCallback(ErrorCallbackGLFW);

	// OpenGL 4.5 with forward compatibility (removed deprecated stuff)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
	glfwWindowHint(GLFW_DEPTH_BITS, GLFW_DONT_CARE); // no need for depth buffer

	// OpenGL Debug context.
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	int width = 1920;
	int height = 1080;

	m_window = glfwCreateWindow(width, height, "<Add fancy title here>", nullptr, nullptr);
	if (!m_window)
	{
		glfwTerminate();
		throw std::exception("Failed to create glfw window!");
	}

	glfwMakeContextCurrent(m_window);
	glfwSetWindowSizeCallback(m_window, WindowResizeCallback);
	 s_resizeHandlers.insert(std::make_pair(m_window, std::vector<ResizeHandlerFunc>()));


	// Init glew now since the GL context is ready.
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK)
		throw std::string("Error: ") + reinterpret_cast<const char*>(glewGetErrorString(err));

	// glew has a problem with GLCore and gives sometimes a GL_INVALID_ENUM
	// http://stackoverflow.com/questions/10857335/opengl-glgeterror-returns-invalid-enum-after-call-to-glewinit
	// Ignore it:
	glGetError();

#ifdef _DEBUG
	gl::ActivateGLDebugOutput(gl::DebugSeverity::MEDIUM);
#endif
	
	// Disable V-Sync
	glfwSwapInterval(0);

	GetGLFWKeystates();
}

OutputWindow::~OutputWindow(void)
{
	SAFE_DELETE(m_screenTri);

	glfwDestroyWindow(m_window);
	m_window = nullptr;
	glfwTerminate();
}	

void OutputWindow::ChangeWindowSize(const ei::UVec2& newResolution)
{
	glfwSetWindowSize(m_window, newResolution.x, newResolution.y);
	GL_CALL(glViewport, 0, 0, newResolution.x, newResolution.y);
}

/// Polls events to keep the window responsive.
void OutputWindow::PollWindowEvents()
{
	GetGLFWKeystates();
	glfwPollEvents();
}

bool OutputWindow::IsWindowAlive()
{
	return !glfwWindowShouldClose(m_window);
}

void OutputWindow::GetGLFWKeystates()
{
	for(unsigned int i=0; i< GLFW_KEY_LAST; ++i)
		oldGLFWKeystates[i] = glfwGetKey(m_window, i);
}

bool OutputWindow::WasButtonPressed(unsigned int glfwKey)
{
	if(glfwKey >= GLFW_KEY_LAST)
		return false;

	return glfwGetKey(m_window, glfwKey) == GLFW_PRESS && oldGLFWKeystates[glfwKey] == GLFW_RELEASE;
}

void OutputWindow::SetTitle(const std::string& windowTitle)
{
	glfwSetWindowTitle(m_window, windowTitle.c_str());
}

void OutputWindow::Present()
{
	glfwSwapBuffers(m_window);
}


ei::UVec2 OutputWindow::GetFramebufferSize()
{
	ei::IVec2 resolution;
	glfwGetFramebufferSize(m_window, &resolution.x, &resolution.y);
	return ei::UVec2(resolution);
}

HWND OutputWindow::GetWindowHandle()
{
	return glfwGetWin32Window(m_window);
}