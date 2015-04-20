#include "application.hpp"

#include <algorithm>

#include "outputwindow.hpp"

#include "utilities/utils.hpp"

#include "rendering/renderer.hpp"

#include "scene/scene.hpp"
#include "scene/model.hpp"
#include "scene/sceneentity.hpp"

#include "camera/interactivecamera.hpp"

#include "shaderfilewatcher.hpp"
#include "anttweakbarinterface.hpp"

#include "glhelper/utils/pathutils.hpp"

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
	m_camera.reset(new InteractiveCamera(m_window->GetGLFWWindow(), ei::Vec3(0.0f, 2.5f, 5.0f), ei::Vec3(0.0f),
		static_cast<float>(resolution.x) / resolution.y, 0.1f, 1000.0f, 60.0f, ei::Vec3(0, 1, 0)));

	// Scene
	LOG_INFO("\nLoad scene ...");
	m_scene.reset(new Scene());

	// Renderer.
	LOG_INFO("\nSetup renderer ...");
	m_renderer.reset(new Renderer(m_scene, m_window->GetResolution()));

	// Watch shader dir.
	ShaderFileWatcher::Instance().SetShaderWatchDirectory("shader");

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
	char oldCurrentPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, oldCurrentPath);

	char fileName[MAX_PATH] = "";

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	
	if (GetOpenFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		return fileName;
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

	char szFileName[MAX_PATH];
	strcpy(szFileName, defaultName.c_str());

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = fileEnding.c_str();

	if (GetSaveFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		return szFileName;
	}
	else
	{
		return "";
	}
}

void Application::ChangeEntityCount(unsigned int entityCount)
{
	size_t oldEntityCount = m_scene->GetEntities().size();
	m_scene->GetEntities().resize(entityCount);

	for (size_t i = entityCount; i < oldEntityCount; ++i)
	{
		std::string namePrefix = "Entity" + std::to_string(i) + "_";
		m_tweakBar->Remove(namePrefix + "LoadFromFile");
		m_tweakBar->Remove(namePrefix + "Filename");
		m_tweakBar->Remove(namePrefix + "Position");
		m_tweakBar->Remove(namePrefix + "Orientation");
		m_tweakBar->Remove(namePrefix + "Scale");
		m_tweakBar->Remove(namePrefix + "Movement");
		m_tweakBar->Remove(namePrefix + "Rotation");
		m_tweakBar->Remove(namePrefix + "Transform");
	}
	for (size_t i = oldEntityCount; i < entityCount; ++i)
	{
		std::string namePrefix = "Entity" + std::to_string(i) + "_";
		std::string entityGroup = "Entity" + std::to_string(i);
		std::string groupSetting = " group=" + entityGroup + " ";

		m_tweakBar->AddButton(namePrefix + "LoadFromFile", [&, i]() {
			std::string filename = OpenFileDialog();
			if (!filename.empty())
				m_scene->GetEntities()[i].LoadModel(filename);
			}, groupSetting + "label=LoadFromFile");

		m_tweakBar->AddReadWrite<std::string>(namePrefix + "Filename", [=]()->std::string { return m_scene->GetEntities()[i].GetModel() != nullptr ? m_scene->GetEntities()[i].GetModel()->GetOriginFilename() : ""; },
			[=](const std::string& str){ m_scene->GetEntities()[i].LoadModel(str); }, groupSetting + "label=Type readonly=true");

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Position", [=](){ return m_scene->GetEntities()[i].GetPosition(); },
			[=](const ei::Vec3& v){ m_scene->GetEntities()[i].SetPosition(v); }, groupSetting + " label=Position", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Quaternion>(namePrefix + "Orientation", [=](){ return m_scene->GetEntities()[i].GetOrientation(); },
			[=](const ei::Quaternion& q){ m_scene->GetEntities()[i].SetOrientation(q); }, groupSetting + " label=Orientation");

		m_tweakBar->AddReadWrite<float>(namePrefix + "Scale", [=](){ return m_scene->GetEntities()[i].GetScale(); },
			[=](float s){ m_scene->GetEntities()[i].SetScale(s); }, groupSetting + " label=Scale min=0.01 max=100.0 step=0.01");

		m_tweakBar->AddSeperator(namePrefix + "Transform", groupSetting);
	
		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Movement", [=](){ return m_scene->GetEntities()[i].GetMovementSpeed(); },
			[=](const ei::Vec3& v){ m_scene->GetEntities()[i].SetMovementSpeed(v); }, groupSetting + " label=Movement", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Rotation", [=](){ return m_scene->GetEntities()[i].GetRotationSpeed(); },
			[=](const ei::Vec3& v){ m_scene->GetEntities()[i].SetRotationSpeed(v); }, groupSetting + " label=Rotation", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->SetGroupProperties(entityGroup, "Entities", entityGroup, false);
	}
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

	size_t oldLightCount = m_scene->GetLights().size();
	m_scene->GetLights().resize(lightCount);


	for (size_t i = lightCount; i < oldLightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		m_tweakBar->Remove(namePrefix + "Type");
		m_tweakBar->Remove(namePrefix + "Intensity");
		m_tweakBar->Remove(namePrefix + "Position");
		m_tweakBar->Remove(namePrefix + "Direction");
		m_tweakBar->Remove(namePrefix + "HalfAngle");	
		m_tweakBar->Remove(namePrefix + "ShadowMapResolution");
		m_tweakBar->Remove(namePrefix + "NormalBias");
		m_tweakBar->Remove(namePrefix + "Bias");
	}
	for (size_t i = oldLightCount; i < lightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		std::string lightGroup = "Light" + std::to_string(i);
		std::string groupSetting = " group=" + lightGroup + " ";
		m_tweakBar->AddReadWrite<int>(namePrefix + "Type", [=](){ return static_cast<int>(m_scene->GetLights()[i].type); },
			[=](const int& i){ m_scene->GetLights()[i].type = static_cast<Light::Type>(i); }, groupSetting + "label=Type", static_cast<AntTweakBarInterface::TypeHint>(lightEnumType));

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Intensity", [=](){ return m_scene->GetLights()[i].intensity; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].intensity = v; }, groupSetting + " label=Intensity", AntTweakBarInterface::TypeHint::HDRCOLOR);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Position", [=](){ return m_scene->GetLights()[i].position; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].position = v; }, groupSetting + " label=Position", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Direction", [=](){ return m_scene->GetLights()[i].direction; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].direction = v; }, groupSetting + " label=Direction");

		m_tweakBar->AddReadWrite<float>(namePrefix + "HalfAngle", [=](){ return m_scene->GetLights()[i].halfAngle; },
			[=](const float& f){ m_scene->GetLights()[i].halfAngle = f; }, groupSetting + " label=HalfAngle min=0.01 max=1.57 step=0.01");

		m_tweakBar->AddReadWrite<int>(namePrefix + "ShadowMapResolution", [=](){ return m_scene->GetLights()[i].shadowMapResolution; },
			[=](const int& res){ m_scene->GetLights()[i].shadowMapResolution = res; }, groupSetting + " label=\"Shadow Map Resolution\" min=16 max=2048 step=16");

		m_tweakBar->AddReadWrite<float>(namePrefix + "NormalBias", [=](){ return m_scene->GetLights()[i].normalOffsetShadowBias; },
			[=](const float& f){ m_scene->GetLights()[i].normalOffsetShadowBias = f; }, groupSetting + " label=\"NormalOffset Shadow Bias\" min=0.00 max=100.0 step=0.001");

		m_tweakBar->AddReadWrite<float>(namePrefix + "Bias", [=](){ return m_scene->GetLights()[i].shadowBias; },
			[=](const float& f){ m_scene->GetLights()[i].shadowBias = f; }, groupSetting + " label=\"Shadow Bias\" min=0.00 max=1.0 step=0.00001");

		m_tweakBar->SetGroupProperties(lightGroup, "Lights", lightGroup, false);
	}
}

void Application::SetupTweakBarBinding()
{
	m_tweakBar = std::make_unique<AntTweakBarInterface>(m_window->GetGLFWWindow());

	m_tweakBar->AddReadOnly("Frametime (ms)", [&](){ return std::to_string(m_timeSinceLastUpdate.GetMilliseconds()); });
	m_tweakBar->AddButton("Save Settings", [&](){ m_tweakBar->SaveReadWriteValuesToJSON(SaveFileDialog("settings.json", ".json")); });
	m_tweakBar->AddButton("Load Settings", [&](){ m_tweakBar->LoadReadWriteValuesToJSON(OpenFileDialog()); });
	

	// Camera
	m_tweakBar->AddReadWrite<float>("Camera Speed", [&](){ return m_camera->GetMoveSpeed(); }, [&](float f){ return m_camera->SetMoveSpeed(f); }, " min=0.01 max=100 step=0.01 label=Speed group=Camera");
	m_tweakBar->AddReadWrite<ei::Vec3>("Camera Direction", [&](){ return m_camera->GetDirection(); }, [&](const ei::Vec3& v){ return m_camera->SetLookAt(v + m_camera->GetPosition()); }, "label=Direction group=Camera");
	m_tweakBar->AddReadWrite<ei::Vec3>("Camera Position", [&](){ return m_camera->GetPosition(); }, [&](const ei::Vec3& v){ return m_camera->SetPosition(v); }, "label=Position group=Camera", AntTweakBarInterface::TypeHint::POSITION);

	// Tonemap
	m_tweakBar->AddReadWrite<float>("Exposure", [&](){ return m_renderer->GetExposure(); }, [&](float f){ return m_renderer->SetExposure(f); }, " min=0.1 max=100 step=0.05 ");

	/// Light cache settings
	m_tweakBar->AddSeperator("Light Cache settings");
	m_tweakBar->AddReadWrite<bool>("Track Light Cache Count", [&](){ return m_renderer->GetReadLightCacheCount(); }, 
																[&](bool b){ return m_renderer->SetReadLightCacheCount(b); });
	m_tweakBar->AddReadOnly("#Active Caches", [&](){ return std::to_string(m_renderer->GetLightCacheActiveCount()); });


	m_tweakBar->AddSeperator("Scene Settings");

	// Entity settings
	std::function<void(const int&)> changeEntityCount = std::bind(&Application::ChangeEntityCount, this, std::placeholders::_1);
	m_tweakBar->AddReadWrite<int>("Entity Count", [&](){ return static_cast<int>(m_scene->GetEntities().size()); }, changeEntityCount, " min=1 max=16 step=1 group=Entities");

	// Light settings
	std::function<void(const int&)> changeLightCount = std::bind(&Application::ChangeLightCount, this, std::placeholders::_1);
	m_tweakBar->AddReadWrite<int>("Light Count", [&](){ return static_cast<int>(m_scene->GetLights().size()); }, changeLightCount, " min=1 max=16 step=1 group=Lights");
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