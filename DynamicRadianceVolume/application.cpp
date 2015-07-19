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

#include "patheditor.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>



Application::Application(int argc, char** argv) :
	m_detachViewFromCameraUpdate(false),
	m_tweakBarStatisticGroupSetting(" group=\"TimerStatistics\""),
	m_showTweakBars(true),
	m_cameraFollowPath(false)
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
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader/cachedebug");

	// Resize handler.
	m_window->AddResizeHandler([&](int width, int height){
		m_renderer->OnScreenResize(ei::UVec2(width, height));
		m_camera->SetAspectRatio(static_cast<float>(width) / height);
		m_mainTweakBar->SetWindowSize(width, height);
	});

	SetupMainTweakBarBinding();

	m_cameraPath = std::make_unique<CameraSpline>();
	m_pathEditor = std::make_unique<PathEditor>(*this);

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

void Application::StartPerfRecording()
{
	FrameProfiler::GetInstance().WaitForQueryResults();
	FrameProfiler::GetInstance().Clear();
	FrameProfiler::GetInstance().SetGPUProfilingActive(true);
}
void Application::StopPerfRecording(const std::string& resulftCSVFilename)
{
	FrameProfiler::GetInstance().SetGPUProfilingActive(false);
	FrameProfiler::GetInstance().WaitForQueryResults();
	FrameProfiler::GetInstance().SaveToCSV(resulftCSVFilename);
}

void Application::Update()
{
	m_window->PollWindowEvents();

	ShaderFileWatcher::Instance().Update();

	m_camera->Update(m_timeSinceLastUpdate);
	m_pathEditor->Update(m_timeSinceLastUpdate);
	Input();

	m_scene->Update(m_timeSinceLastUpdate);

	m_window->SetTitle("time per frame " +
		std::to_string(m_timeSinceLastUpdate.GetMilliseconds()) + "ms (FPS: " + std::to_string(1.0f / m_timeSinceLastUpdate.GetSeconds()) + ")");

	if (m_tweakBarStatisticEntries.size() != FrameProfiler::GetInstance().GetAllRecordedEvents().size())
	{
		// Remove old entries.
		for (const std::string& entry : m_tweakBarStatisticEntries)
			m_mainTweakBar->Remove(entry);
		m_tweakBarStatisticEntries.clear();
		

		// Add
		for (const auto& entry : FrameProfiler::GetInstance().GetAllRecordedEvents())
		{
			std::string name = entry.first;
			m_mainTweakBar->AddReadOnly(name, [name]{
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

	if (m_cameraFollowPath)
	{
		ei::Vec3 position = m_camera->GetPosition();
		ei::Vec3 direction = m_camera->GetDirection();

		m_cameraPath->Move(static_cast<float>(m_timeSinceLastUpdate.GetSeconds()));
		m_cameraPath->Evaluate(position, direction);
		m_camera->SetPosition(position);
		m_camera->SetDirection(direction);
	}
}

void Application::Draw()
{
	m_renderer->Draw(*m_camera, m_detachViewFromCameraUpdate);
	if (m_detachViewFromCameraUpdate)
		m_frustumOutlineRenderer->Draw();
	if (m_showTweakBars)
	{
		AntTweakBarInterface::Draw();
	}
	m_window->Present();

	FrameProfiler::GetInstance().OnFrameEnd();
}

static std::string UIntToMinLengthString(int _number, int _minDigits)
{
	int zeros = std::max(0, _minDigits - static_cast<int>(ceil(log10(_number + 1))));
	std::string out = std::string(zeros, '0') + std::to_string(_number);
	return out;
}


void Application::Input()
{
	if (m_window->WasButtonPressed(GLFW_KEY_F10))
	{
		auto screenSize = m_window->GetFramebufferSize();
		char* pixels = new char[3 * screenSize.x * screenSize.y];

		glReadPixels(0, 0, screenSize.x, screenSize.y, GL_RGB, GL_UNSIGNED_BYTE, pixels);

		time_t t = time(0);   // get time now
		struct tm* now = localtime(&t);
		std::string date = UIntToMinLengthString(now->tm_mon + 1, 2) + "." + UIntToMinLengthString(now->tm_mday, 2) + " " +
							UIntToMinLengthString(now->tm_hour, 2) + ";" + UIntToMinLengthString(now->tm_min, 2) + ";" + std::to_string(now->tm_sec) + " ";
		std::string filename = "../screenshots/" + date + " screenshot.png";

		// Flip on y axis
		for (unsigned int y = 0; y < screenSize.y / 2; ++y)
		{
			for (unsigned int x = 0; x < screenSize.x; ++x)
			{
				unsigned int flippedY = screenSize.y - y - 1;
				std::swap(pixels[(x + y * screenSize.x) * 3 + 0], pixels[(x + flippedY * screenSize.x) * 3 + 0]);
				std::swap(pixels[(x + y * screenSize.x) * 3 + 1], pixels[(x + flippedY * screenSize.x) * 3 + 1]);
				std::swap(pixels[(x + y * screenSize.x) * 3 + 2], pixels[(x + flippedY * screenSize.x) * 3 + 2]);
			}
		}
		stbi_write_png(filename.c_str(), screenSize.x, screenSize.y, 3, pixels, screenSize.x*3);
		
		delete[] pixels;
	}
	if (m_window->WasButtonPressed(GLFW_KEY_F11))
		m_showTweakBars = !m_showTweakBars;
}