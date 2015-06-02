#include "application.hpp"

#include <algorithm>

#include "outputwindow.hpp"

#include "rendering/renderer.hpp"
#include "rendering/frustumoutlines.hpp"
#include "scene/scene.hpp"

#include "camera/interactivecamera.hpp"

#include "shaderreload/shaderfilewatcher.hpp"
#include "anttweakbarinterface.hpp"
#include "frameprofiler.hpp"

#undef APIENTRY
#include <windows.h>
#include <shlwapi.h>


Application::Application(int argc, char** argv) :
	m_detachViewFromCameraUpdate(false),
	m_tweakBarStatisticGroupSetting(" group=\"TimerStatistics\"")
{
	// Logger init.
	Logger::g_logger.Initialize(new Logger::FilePolicy("log.txt"));

	// Window...
	LOG_INFO("Init window ...");
	m_window.reset(new OutputWindow());

	// Create "global" camera.
	auto resolution = m_window->GetFramebufferSize();
	m_camera.reset(new InteractiveCamera(m_window->GetGLFWWindow(), ei::Vec3(0.0f, 2.5f, 5.0f), -ei::Vec3(0.0f, 2.5f, 5.0f),
		static_cast<float>(resolution.x) / resolution.y, 0.1f, 1000.0f, 60.0f, ei::Vec3(0, 1, 0)));

	// Scene
	LOG_INFO("\nLoad scene ...");
	m_scene.reset(new Scene());

	// Renderer.
	LOG_INFO("\nSetup renderer ...");
	m_renderer.reset(new Renderer(m_scene, m_window->GetFramebufferSize()));	

	m_frustumOutlineRenderer = std::make_unique<FrustumOutlines>();

	// Watch shader dir.
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader");

	// Resize handler.
	m_window->AddResizeHandler([&](int width, int height){
		m_renderer->OnScreenResize(ei::UVec2(width, height));
		m_camera->SetAspectRatio(static_cast<float>(width) / height);
		m_tweakBar->SetWindowSize(width, height);
	});

	SetupTweakBarBinding();

	// Default settings:
	ChangeEntityCount(1);
	//m_scene->GetEntities()[0].LoadModel("../models/sanmiguel/san-miguel.obj");
	m_scene->GetEntities()[0].LoadModel("../models/test0/test0.obj");
	//m_scene->GetEntities()[0].LoadModel("../models/cryteksponza/sponza.obj");

	ChangeLightCount(1);
	m_scene->GetLights()[0].type = Light::Type::SPOT;
	m_scene->GetLights()[0].intensity = ei::Vec3(100.0f, 100.0f, 100.0f);
	m_scene->GetLights()[0].position = ei::Vec3(0.0f, 1.7f, 3.3f);
	m_scene->GetLights()[0].direction = ei::Vec3(0.0f, 0.0f, -1.0f);
	m_scene->GetLights()[0].halfAngle = 30.0f * (ei::PI / 180.0f);	
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

std::string Application::OpenFileDialog()
{
	// Openfiledialog changes relative paths. Save current directory to restore it later.
	char oldCurrentPath[MAX_PATH] = "";
	GetCurrentDirectoryA(MAX_PATH, oldCurrentPath);

	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	
	if (GetOpenFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		char relativePath[MAX_PATH];
		if (PathRelativePathToA(relativePath, oldCurrentPath, FILE_ATTRIBUTE_DIRECTORY, filename, FILE_ATTRIBUTE_NORMAL))
			return relativePath;
		else
			return filename;
	}
	else
	{
		return "";
	}
}

std::string Application::SaveFileDialog(const std::string& defaultName, const std::string& fileEnding)
{
	// Openfiledialog changes relative paths. Save current directory to restore it later.
	char oldCurrentPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, oldCurrentPath);

	char filename[MAX_PATH];
	strcpy(filename, defaultName.c_str());

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = fileEnding.c_str();

	if (GetSaveFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		char relativePath[MAX_PATH];
		if (PathRelativePathToA(relativePath, oldCurrentPath, FILE_ATTRIBUTE_DIRECTORY, filename, FILE_ATTRIBUTE_NORMAL))
			return relativePath;
		else
			return filename;
	}
	else
	{
		return "";
	}
}

void Application::Run()
{
	// Main loop
	ezStopwatch mainLoopStopWatch;
	while (m_window->IsWindowAlive())
	{
		m_timeSinceLastUpdate = mainLoopStopWatch.GetRunningTotal();
		mainLoopStopWatch.StopAndReset();
		mainLoopStopWatch.Resume();

		Update();
		Draw();
	}
}

void Application::Update()
{
	m_window->PollWindowEvents();

	ShaderFileWatcher::Instance().Update();

	m_camera->Update(m_timeSinceLastUpdate);
	Input();

	m_scene->Update(m_timeSinceLastUpdate);

	m_window->SetTitle("time per frame " +
		std::to_string(m_timeSinceLastUpdate.GetMilliseconds()) + "ms (FPS: " + std::to_string(1.0f / m_timeSinceLastUpdate.GetSeconds()) + ")");

	if (m_tweakBarStatisticEntries.size() != FrameProfiler::GetInstance().GetAllRecordedEvents().size())
	{
		// Remove old entries.
		for (const std::string& entry : m_tweakBarStatisticEntries)
			m_tweakBar->Remove(entry);
		m_tweakBarStatisticEntries.clear();

		// Add
		for (const auto& entry : FrameProfiler::GetInstance().GetAllRecordedEvents())
		{
			std::string name = entry.first;
			m_tweakBar->AddReadOnly(name, [name]{
					auto it = std::find_if(FrameProfiler::GetInstance().GetAllRecordedEvents().begin(), FrameProfiler::GetInstance().GetAllRecordedEvents().end(),
											[name](const FrameProfiler::EventList& v){ return v.first == name; });
					if (it != FrameProfiler::GetInstance().GetAllRecordedEvents().end() && !it->second.empty())
						return std::to_string(it->second.back().duration / 1000.0);
					else
						return std::string("");
				}, m_tweakBarStatisticGroupSetting);
			m_tweakBarStatisticEntries.push_back(name);
		}
	}
}

void Application::Draw()
{
	m_renderer->Draw(*m_camera, m_detachViewFromCameraUpdate);
	if (m_detachViewFromCameraUpdate)
		m_frustumOutlineRenderer->Draw();
	m_tweakBar->Draw();
	m_window->Present();

	FrameProfiler::GetInstance().OnFrameEnd();
}

void Application::Input()
{
}