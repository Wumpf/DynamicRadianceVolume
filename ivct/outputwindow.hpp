#pragma once

#include <glhelper/shaderobject.hpp>
#include <GLFW/glfw3.h>
#include <ei/vector.hpp>

struct GLFWwindow;
namespace gl
{
	class Texture2D;
	class ScreenAlignedTriangle;
}

/// A GLFW based output window
class OutputWindow
{
public:
	OutputWindow();
	~OutputWindow();

	void ChangeWindowSize(const ei::UVec2& newResolution);
	
	/// Polls events to keep the window responsive.
	void PollWindowEvents();

	/// Returns false if the window was or wants to be closed.
	bool IsWindowAlive();

	bool WasButtonPressed(unsigned int glfwKey);

	/// Returns GLFW window.
	GLFWwindow* GetGLFWWindow() { return window; }

	void Present();

	void SetTitle(const std::string& windowTitle);

	ei::UVec2 GetResolution();

private:
	void GetGLFWKeystates();
	
	GLFWwindow* window;
	gl::ShaderObject displayHDR;
	gl::ScreenAlignedTriangle* screenTri;

	int oldGLFWKeystates[GLFW_KEY_LAST];
};

