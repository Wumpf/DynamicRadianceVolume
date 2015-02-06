#include "outputwindow.hpp"

#include "glhelper/texture2d.hpp"
#include "glhelper/screenalignedtriangle.hpp"

#include "utilities/logger.hpp"
#include "utilities/utils.hpp"

#include <iostream>
#include <cassert>
#include <memory>
#include <string>


static void ErrorCallbackGLFW(int error, const char* description)
{
	LOG_ERROR("GLFW error, code " + std::to_string(error) + " desc: \"" + description + "\"");
}

OutputWindow::OutputWindow() :
	displayHDR("displayHDR")
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

	int width = 1024;
	int height = 768;

	window = glfwCreateWindow(width, height, "<Add fancy title here>", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		throw std::exception("Failed to create glfw window!");
	}

	glfwMakeContextCurrent(window);

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
	gl::ActivateGLDebugOutput(gl::DebugSeverity::LOW);
#endif
	
	// Disable V-Sync
	glfwSwapInterval(0);

	GetGLFWKeystates();
}

OutputWindow::~OutputWindow(void)
{
	SAFE_DELETE(screenTri);

	glfwDestroyWindow(window);
	window = nullptr;
	glfwTerminate();
}	

void OutputWindow::ChangeWindowSize(const ei::UVec2& newResolution)
{
	glfwSetWindowSize(window, newResolution.x, newResolution.y);
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
	return !glfwWindowShouldClose(window);
}

void OutputWindow::GetGLFWKeystates()
{
	for(unsigned int i=0; i< GLFW_KEY_LAST; ++i)
		oldGLFWKeystates[i] = glfwGetKey(window, i);
}

bool OutputWindow::WasButtonPressed(unsigned int glfwKey)
{
	if(glfwKey >= GLFW_KEY_LAST)
		return false;

	return glfwGetKey(window, glfwKey) == GLFW_PRESS && oldGLFWKeystates[glfwKey] == GLFW_RELEASE;
}

void OutputWindow::SetTitle(const std::string& windowTitle)
{
	glfwSetWindowTitle(window, windowTitle.c_str());
}

void OutputWindow::Present()
{
	glfwSwapBuffers(window);
}


ei::UVec2 OutputWindow::GetResolution()
{
	ei::IVec2 resolution;
	glfwGetWindowSize(window, &resolution.x, &resolution.y);
	return ei::UVec2(resolution);
}