#include "anttweakbarinterface.hpp"
#include "utilities/logger.hpp"

#include <glhelper/samplerobject.hpp>

#include <GLFW/glfw3.h>

void TwEventMouseButtonGLFW3(GLFWwindow* window, int button, int action, int mods){ TwEventMouseButtonGLFW(button, action); }
void TwEventMousePosGLFW3(GLFWwindow* window, double xpos, double ypos){ TwMouseMotion(int(xpos), int(ypos)); }
void TwEventMouseWheelGLFW3(GLFWwindow* window, double xoffset, double yoffset){ TwEventMouseWheelGLFW(static_cast<int>(yoffset)); }
void TwEventKeyGLFW3(GLFWwindow* window, int key, int scancode, int action, int mods){ TwEventKeyGLFW(key, action); }
void TwEventCharGLFW3(GLFWwindow* window, int codepoint){ TwEventCharGLFW(codepoint, GLFW_PRESS); }

AntTweakBarInterface::AntTweakBarInterface(GLFWwindow* glfwWindow) :
	m_tweakBar(nullptr)
{
	if (!TwInit(TW_OPENGL_CORE, NULL))
	{
		LOG_ERROR("AntTweakBar initialization failed: " << TwGetLastError());
	}
	else
	{
		// Set GLFW event callbacks
		int width, height;
		glfwGetWindowSize(glfwWindow, &width, &height); 
		TwWindowSize(width, height);

		glfwSetMouseButtonCallback(glfwWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW3);
		glfwSetCursorPosCallback(glfwWindow, (GLFWcursorposfun)TwEventMousePosGLFW3);
		glfwSetScrollCallback(glfwWindow, (GLFWscrollfun)TwEventMouseWheelGLFW3);
		glfwSetKeyCallback(glfwWindow, (GLFWkeyfun)TwEventKeyGLFW3);
		glfwSetCharCallback(glfwWindow, (GLFWcharfun)TwEventCharGLFW3);



		// Create a tweak bar
		m_tweakBar = TwNewBar("TweakBar");

	/*	static const ezSizeU32 tweakBarSize(330, 600);
		std::stringBuilder stringBuilder;
		stringBuilder.Format(" TweakBar size='330 600' ", tweakBarSize.width, tweakBarSize.height);
		TwDefine(stringBuilder.GetData());
		stringBuilder.Format(" TweakBar position='%i %i' ", GeneralConfig::g_ResolutionWidth - tweakBarSize.width - 20, 20);
		TwDefine(stringBuilder.GetData());
		*/


		TwDefine(" TweakBar refresh=0.2 ");
		TwDefine(" TweakBar contained=true "); // TweakBar must be inside the window.
		//TwDefine(" TweakBar alpha=200 ");
	}
}

AntTweakBarInterface::~AntTweakBarInterface(void)
{
	if (m_tweakBar)
	{
		TwTerminate();
	}

	for (auto entry : m_entries)
		delete entry;
}

void AntTweakBarInterface::CheckTwError()
{
	const char* errorDesc = TwGetLastError();
	if (errorDesc != NULL)
		LOG_ERROR("Tw error: " << errorDesc);
}

void AntTweakBarInterface::AddButton(const std::string& name, std::function<void()>& triggerCallback, const std::string& twDefines)
{
	EntryButton* entry = new EntryButton();
	entry->name = name;
	entry->triggerCallback = triggerCallback;
	m_entries.push_back(entry);

	TwButtonCallback fkt = [](void* entry) {
		static_cast<EntryButton*>(entry)->triggerCallback();
	};

	TwAddButton(m_tweakBar, name.c_str(), fkt, m_entries.back(), twDefines.c_str());
	CheckTwError();
}

void AntTweakBarInterface::AddReadOnly(const std::string& name, std::function<std::string()>& getValue, const std::string& twDefines)
{
	EntryReadOnly* entry = new EntryReadOnly();
	entry->name = name;
	entry->getValue = getValue;
	m_entries.push_back(entry);

	TwGetVarCallback getFkt = [](void *value, void *clientData) {
		std::string* destPtr = static_cast<std::string*>(value);
		TwCopyStdStringToLibrary(*destPtr, static_cast<EntryReadOnly*>(clientData)->getValue());
	};

	TwAddVarCB(m_tweakBar, name.c_str(), TW_TYPE_STDSTRING, NULL, getFkt, m_entries.back(), twDefines.c_str());
	CheckTwError();
}


void AntTweakBarInterface::AddSeperator(const std::string& name, const std::string& twDefines)
{
	TwAddSeparator(m_tweakBar, name.c_str(), twDefines.c_str());
	CheckTwError();
}

void AntTweakBarInterface::Draw()
{
	gl::SamplerObject::ResetBinding(0);
	TwDraw();
}