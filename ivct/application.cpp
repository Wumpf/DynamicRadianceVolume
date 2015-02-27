#include "application.hpp"

#include <algorithm>

#include "outputwindow.hpp"

#include "utilities/utils.hpp"

#include "rendering/renderer.hpp"
#include "scene/scene.hpp"
#include "camera/interactivecamera.hpp"

#include "shaderfilewatcher.hpp"
#include "anttweakbarinterface.hpp"

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
	m_camera.reset(new InteractiveCamera(m_window->GetGLFWWindow(), ei::Vec3(0.0f, 2.5f, -5.0f), ei::Vec3(0.0f),
		static_cast<float>(resolution.x) / resolution.y, 0.1f, 10000.0f, 60.0f, ei::Vec3(0, 1, 0)));

	// Scene
	LOG_INFO("\nLoad scene ...");
	m_scene.reset(new Scene());

	// TODO: Some kind of runtime/data system is needed here.
	m_scene->AddModel("../models/test0/test0.obj");
	Light spot;
	spot.type = Light::Type::SPOT;
	spot.intensity = ei::Vec3(100.0f, 90.0f, 90.0f); 
	spot.position = ei::Vec3(0.0f, 2.5f, -2.5f);
	spot.direction = ei::normalize(-spot.position);
	spot.halfAngle = 30.0f * (ei::PI / 180.0f);
	m_scene->AddLight(spot);
	spot.position = ei::Vec3(1.0f, 2.5f, -2.5f);
	spot.intensity = ei::Vec3(70.0f, 70.0f, 80.0f);
	spot.direction = ei::normalize(ei::Vec3(1.0f, 2.5f, 0.0f) - spot.position);
	m_scene->AddLight(spot);

	// Renderer.
	LOG_INFO("\nSetup renderer ...");
	m_renderer.reset(new Renderer(m_scene, m_window->GetResolution()));

	// Watch shader dir.
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader");

	SetupTweakBarBinding();
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

void Application::SetupTweakBarBinding()
{
	m_tweakBar = std::make_unique<AntTweakBarInterface>(m_window->GetGLFWWindow());


	// Define light type
	TwEnumVal lightTypes[] = // array used to describe the Scene::AnimMode enum values
	{
		{ static_cast<int>(Light::Type::SPOT), "Spot" }
	};
	TwType lightEnumType = TwDefineEnum("LightType", lightTypes, ArraySize(lightTypes));  // create a new TwType associated to the enum defined by the modeEV array

	// Type for HDR color
	TwStructMember hdrColorRGBMembers[] =
	{
		{ "R", TW_TYPE_FLOAT, 0, " step=0.1" },
		{ "G", TW_TYPE_FLOAT, sizeof(float), " step=0.1" },
		{ "B", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1" },
	};
	TwType hdrColorRGBStructType = TwDefineStruct("HDR Color", hdrColorRGBMembers, ArraySize(hdrColorRGBMembers), sizeof(ei::Vec3), nullptr, nullptr);

	// Type for position
	TwStructMember positionMembers[] =
	{
		{ "X", TW_TYPE_FLOAT, 0, " step=0.1" },
		{ "Y", TW_TYPE_FLOAT, sizeof(float), " step=0.1" },
		{ "Z", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1" },
	};
	TwType positionStructType = TwDefineStruct("Position", positionMembers, ArraySize(positionMembers), sizeof(ei::Vec3), nullptr, nullptr);

	// Define light struct
	TwStructMember lightMembers[] =
	{
		{ "Type", lightEnumType, offsetof(Light, type), " " },
		{ "Intensity", hdrColorRGBStructType, offsetof(Light, intensity), " " },
		{ "Position", positionStructType, offsetof(Light, position), " " },
		{ "Direction", TW_TYPE_DIR3F, offsetof(Light, direction), " " },
		{ "HalfAngle", TW_TYPE_FLOAT, offsetof(Light, halfAngle), " min=0 max=1.57 step=0.01" },
	};
	TwType lightStructType = TwDefineStruct("Light", lightMembers, ArraySize(lightMembers), sizeof(Light), nullptr, nullptr);



	m_tweakBar->AddReadOnly("Frametime (ms)", std::function<std::string(void)>([&](){ return std::to_string(m_timeSinceLastUpdate.GetMilliseconds()); }));

	// Light settings
	m_tweakBar->AddReadWrite("Light Count", std::function<std::uint32_t(void)>([&](){ return static_cast<std::uint32_t>(m_scene->GetLights().size()); }),
		std::function<void(const std::uint32_t&)>([&, lightStructType](const std::uint32_t& lightCount)
		{
			// The light structs do not work properly with the callback version.
			// Therefore we must use the variable-reference version. 
			// This is super dangerous, as we give away pointer so elements of std::vector. On each resize of std::vector, the list must be rebuilt!
			// Changes to the light count from outside are however not handled at all!
			for (size_t i = 0; i < m_scene->GetLights().size(); ++i)
				m_tweakBar->Remove("Light" + std::to_string(i));

			m_scene->GetLights().resize(lightCount);

			for (size_t i = 0; i < m_scene->GetLights().size(); ++i)
				m_tweakBar->AddReadWrite("Light" + std::to_string(i), m_scene->GetLights()[i], " group=Lights", lightStructType);
		}), " min=1 max=16 step=1");
	for (size_t i = 0; i < m_scene->GetLights().size(); ++i)
		m_tweakBar->AddReadWrite("Light" + std::to_string(i), m_scene->GetLights()[i], " group=Lights", lightStructType);
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

	m_window->SetTitle("time per frame " +
		std::to_string(m_timeSinceLastUpdate.GetMilliseconds()) + "ms (FPS: " + std::to_string(1.0f / m_timeSinceLastUpdate.GetSeconds()) + ")");
}

void Application::Draw()
{
	m_renderer->Draw(*m_camera);
	m_tweakBar->Draw();
	m_window->Present();
}

void Application::Input()
{
}