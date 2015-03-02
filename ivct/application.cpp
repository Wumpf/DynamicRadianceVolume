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

	// Renderer.
	LOG_INFO("\nSetup renderer ...");
	m_renderer.reset(new Renderer(m_scene, m_window->GetResolution()));

	// Watch shader dir.
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader");

	SetupTweakBarBinding();




	ChangeLightCount(2);

	m_scene->GetLights()[0].type = Light::Type::SPOT;
	m_scene->GetLights()[0].intensity = ei::Vec3(100.0f, 90.0f, 90.0f);
	m_scene->GetLights()[0].position = ei::Vec3(0.0f, 2.5f, -2.5f);
	m_scene->GetLights()[0].direction = ei::normalize(-m_scene->GetLights()[0].position);
	m_scene->GetLights()[0].halfAngle = 30.0f * (ei::PI / 180.0f);

	m_scene->GetLights()[1].type = Light::Type::SPOT;
	m_scene->GetLights()[1].position = ei::Vec3(1.0f, 2.5f, -2.5f);
	m_scene->GetLights()[1].intensity = ei::Vec3(70.0f, 70.0f, 80.0f);
	m_scene->GetLights()[1].direction = ei::normalize(ei::Vec3(1.0f, 2.5f, 0.0f) - m_scene->GetLights()[1].position);
	m_scene->GetLights()[1].halfAngle = 30.0f * (ei::PI / 180.0f);

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

void Application::ChangeLightCount(unsigned int lightCount)
{
	// Define light type
	const static TwEnumVal lightTypes[] = // array used to describe the Scene::AnimMode enum values
	{
		{ static_cast<int>(Light::Type::SPOT), "Spot" }
	};
	static TwType lightEnumType = TW_TYPE_UNDEF;
	if (lightEnumType == TW_TYPE_UNDEF)
		lightEnumType = TwDefineEnum("LightType", lightTypes, ArraySize(lightTypes));

	std::size_t oldLightCount = m_scene->GetLights().size();
	m_scene->GetLights().resize(lightCount);


	for (size_t i = lightCount; i < oldLightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		m_tweakBar->Remove(namePrefix + "Type");
		m_tweakBar->Remove(namePrefix + "Intensity");
		m_tweakBar->Remove(namePrefix + "Position");
		m_tweakBar->Remove(namePrefix + "Direction");
		m_tweakBar->Remove(namePrefix + "HalfAngle");		
	}
	for (size_t i = oldLightCount; i < lightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		std::string lightGroup = " group=Light" + std::to_string(i) + " ";
		m_tweakBar->AddReadWrite<int>(namePrefix + "Type", [=](){ return static_cast<int>(m_scene->GetLights()[i].type); },
			[=](const int& i){ m_scene->GetLights()[i].type = static_cast<Light::Type>(i); }, lightGroup + "label=Type", static_cast<AntTweakBarInterface::TypeHint>(lightEnumType));

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Intensity", [=](){ return m_scene->GetLights()[i].intensity; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].intensity = v; }, lightGroup + " label=Intensity", AntTweakBarInterface::TypeHint::HDRCOLOR);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Position", [=](){ return m_scene->GetLights()[i].position; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].position = v; }, lightGroup + " label=Position", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Direction", [=](){ return m_scene->GetLights()[i].direction; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].direction = v; }, lightGroup + " label=Direction");

		m_tweakBar->AddReadWrite<float>(namePrefix + "HalfAngle", [=](){ return m_scene->GetLights()[i].halfAngle; },
			[=](const float& f){ m_scene->GetLights()[i].halfAngle = f; }, lightGroup + " label=HalfAngle min=0.01 max=1.57 step=0.01 label=HalfAngle");
	}

	m_scene->GetLights().resize(lightCount);
}

void Application::SetupTweakBarBinding()
{
	m_tweakBar = std::make_unique<AntTweakBarInterface>(m_window->GetGLFWWindow());

	// Type for HDR color
	/*std::vector<TwStructMember> hdrColorRGBMembers(
	{
		{ "R", TW_TYPE_FLOAT, 0, " step=0.1" },
		{ "G", TW_TYPE_FLOAT, sizeof(float), " step=0.1" },
		{ "B", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1" },
	});
	TwType hdrColorRGBStructType = m_tweakBar->DefineStruct("HDR Color", hdrColorRGBMembers, sizeof(ei::Vec3));

	// Type for position
	std::vector<TwStructMember>  positionMembers(
	{
		{ "X", TW_TYPE_FLOAT, 0, " step=0.1" },
		{ "Y", TW_TYPE_FLOAT, sizeof(float), " step=0.1" },
		{ "Z", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1" },
	});
	TwType positionStructType = m_tweakBar->DefineStruct("Position", positionMembers, sizeof(ei::Vec3));
	*/
	
	m_tweakBar->AddReadOnly("Frametime (ms)", [&](){ return std::to_string(m_timeSinceLastUpdate.GetMilliseconds()); });
	m_tweakBar->AddButton("Save Settings", [&](){ m_tweakBar->SaveReadWriteValuesToJSON("settings.json"); });
	m_tweakBar->AddButton("Load Settings", [&](){ m_tweakBar->LoadReadWriteValuesToJSON("settings.json"); });

	// Light settings
	std::function<void(const int&)> changeLightCount = std::bind(&Application::ChangeLightCount, this, std::placeholders::_1);
	m_tweakBar->AddReadWrite<int>("Light Count", [&](){ return static_cast<int>(m_scene->GetLights().size()); }, changeLightCount, " min=1 max=16 step=1");
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