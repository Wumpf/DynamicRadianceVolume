#include "application.hpp"

#include <algorithm>

#include "outputwindow.hpp"

#include "utilities/utils.hpp"

#include "rendering/renderer.hpp"
#include "scene/scene.hpp"
#include "camera/interactivecamera.hpp"

#include "shaderfilewatcher.hpp"

#ifdef _WIN32
#undef APIENTRY
#include <windows.h>
#endif

Application::Application(int argc, char** argv)
{
	// Logger init.
	Logger::g_logger.Initialize(new Logger::FilePolicy("log.txt"));

	// Window...
	LOG_INFO("Init window ...");
	m_window.reset(new OutputWindow());

	// Create "global" camera.
	auto resolution = m_window->GetResolution();
	m_camera.reset(new InteractiveCamera(m_window->GetGLFWWindow(), ei::Vec3(0.0f, 5.0f, 10.0f), ei::Vec3(0.0f),
		static_cast<float>(resolution.x) / resolution.y, 0.1f, 10000.0f, 60.0f, ei::Vec3(0, 1, 0)));

	// Scene
	LOG_INFO("\nLoad scene ...");
	m_scene.reset(new Scene());
	m_scene->AddModel("../models/test0/test0.obj");

	// Renderer.
	LOG_INFO("\nSetup renderer ...");
	m_renderer.reset(new Renderer(m_scene, m_window->GetResolution()));

	// Watch shader dir.
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader");
}

Application::~Application()
{
	// Only known method to kill the console.
#ifdef _WIN32
	FreeConsole();
#endif
	Logger::g_logger.Shutdown();

	m_scene.reset();
}

void Application::Run()
{
	// Main loop
	ezStopwatch mainLoopStopWatch;
	while (m_window->IsWindowAlive())
	{
		ezTime timeSinceLastUpdate = mainLoopStopWatch.GetRunningTotal();
		mainLoopStopWatch.StopAndReset();
		mainLoopStopWatch.Resume();

		Update(timeSinceLastUpdate);
		Draw();
	}
}

void Application::Update(ezTime timeSinceLastUpdate)
{
	m_window->PollWindowEvents();

	ShaderFileWatcher::Instance().Update();

	m_camera->Update(timeSinceLastUpdate);
	Input();

	m_window->SetTitle("time per frame " +
		std::to_string(timeSinceLastUpdate.GetMilliseconds()) + "ms (FPS: " + std::to_string(1.0f / timeSinceLastUpdate.GetSeconds()) + ")");
}

void Application::Draw()
{
	m_renderer->Draw(*m_camera);
	m_window->Present();
}

void Application::Input()
{
}