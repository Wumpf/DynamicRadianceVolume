#include <iostream>
#include <memory>
#include <algorithm>

#include "outputwindow.hpp"

#include "utilities/utils.hpp"
#include "Time/Stopwatch.h"

#include "utilities/loggerinit.hpp"

#include "scene/scene.hpp"
#include "camera/interactivecamera.hpp"

#include <glhelper/texture2d.hpp>

#ifdef _WIN32
	#undef APIENTRY
	#define NOMINMAX
	#include <windows.h>
#endif

class Application
{
public:
	Application(int argc, char** argv) : m_screenShotName("")
	{
		// Logger init.
		Logger::g_logger.Initialize(new Logger::FilePolicy("log.txt"));

		// Window...
		LOG_INFO("Init window ...");
		m_window.reset(new OutputWindow());

		// Create "global" camera.
		auto resolution = m_window->GetResolution();
		m_camera.reset(new InteractiveCamera(m_window->GetGLFWWindow(), ei::Vec3(0.0f, 5.0f, 10.0f), ei::Vec3(0.0f),
			static_cast<float>(resolution.x) / resolution.y, 80.0f, ei::Vec3(0,1,0)));

		// Scene
		LOG_INFO("Load scene ...");
		m_scene.reset(new Scene());
		m_scene->AddModel("../models/san-miguel.obj"); // cryteksponza/sponza.obj");
	}

	~Application()
	{
		// Only known method to kill the console.
#ifdef _WIN32
		FreeConsole();
#endif
		Logger::g_logger.Shutdown();
	}

	void Run()
	{
		// Main loop
		ezStopwatch mainLoopStopWatch;
		while(m_window->IsWindowAlive())
		{
			ezTime timeSinceLastUpdate = mainLoopStopWatch.GetRunningTotal();
			mainLoopStopWatch.StopAndReset();
			mainLoopStopWatch.Resume();

			Update(timeSinceLastUpdate);
			Draw();
		}
	}

private:

	void Update(ezTime timeSinceLastUpdate)
	{
		m_window->PollWindowEvents();

		m_camera->Update(timeSinceLastUpdate);
		Input();

		m_window->SetTitle("time per frame " +
			std::to_string(timeSinceLastUpdate.GetMilliseconds()) + "ms (FPS: " + std::to_string(1.0f / timeSinceLastUpdate.GetSeconds()) + ")");
	}

	void Draw()
	{
		GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		m_scene->Draw(*m_camera);
		m_window->Present();
	}

	void Input()
	{
	}

	std::string UIntToMinLengthString(int _number, int _minDigits)
	{
		int zeros = std::max(0, _minDigits - static_cast<int>(ceil(log10(_number+1))));
		std::string out = std::string(zeros, '0') + std::to_string(_number);
		return out;
	}

	std::string m_screenShotName;
	std::unique_ptr<OutputWindow> m_window;
	std::unique_ptr<Scene> m_scene;
	std::unique_ptr<InteractiveCamera> m_camera;
};


#ifdef _CONSOLE
// Console window handler
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (fdwCtrlType == CTRL_CLOSE_EVENT)
	{
		Logger::g_logger.Shutdown(); // To avoid crash with invalid mutex.
		return FALSE;
	}
	else
		return FALSE;
}
#endif

int main(int argc, char** argv)
{
	// Install console handler.
#ifdef _CONSOLE
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

	// Actual application.
	try
	{
		Application application(argc, argv);
		application.Run();
	}
	catch(const std::exception& e)
	{
		std::cerr << "Unhandled exception:\n";
		std::cerr << "Message: " << e.what() << std::endl;
		__debugbreak();
		return 1;
	}
	catch(const std::string& str)
	{
		std::cerr << "Unhandled exception:\n";
		std::cerr << "Message: " << str << std::endl;
		__debugbreak();
		return 1;
	}
	catch(...)
	{
		std::cerr << "Unknown exception!" << std::endl;
		__debugbreak();
		return 1;
	}

	return 0;
}