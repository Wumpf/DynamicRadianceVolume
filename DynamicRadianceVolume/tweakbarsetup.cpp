#include "application.hpp"

#include "outputwindow.hpp"

#include "rendering/renderer.hpp"

#include "scene/scene.hpp"
#include "scene/model.hpp"
#include "scene/sceneentity.hpp"

#include "camera/interactivecamera.hpp"

#include "anttweakbarinterface.hpp"
#include "frameprofiler.hpp"

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

	size_t oldLightCount = m_scene->GetLights().size();
	m_scene->GetLights().resize(lightCount);


	for (size_t i = lightCount; i < oldLightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		m_tweakBar->Remove(namePrefix + "Type");
		m_tweakBar->Remove(namePrefix + "Intensity");
		m_tweakBar->Remove(namePrefix + "Position");
		m_tweakBar->Remove(namePrefix + "Direction");
		m_tweakBar->Remove(namePrefix + "SpotAngle");
		m_tweakBar->Remove(namePrefix + "shadowseparator");
		m_tweakBar->Remove(namePrefix + "ShadowMapRes");
		m_tweakBar->Remove(namePrefix + "NormalBias");
		m_tweakBar->Remove(namePrefix + "Bias");
		m_tweakBar->Remove(namePrefix + "IndirectShadowLod");
		m_tweakBar->Remove(namePrefix + "IndirectShadowMinConeAngle");

	}
	for (size_t i = oldLightCount; i < lightCount; ++i)
	{
		std::string namePrefix = "Light" + std::to_string(i) + "_";
		std::string lightGroup = "Light" + std::to_string(i);
		std::string groupSetting = " group=" + lightGroup + " ";
		m_tweakBar->AddEnum(namePrefix + "Type", "LightType", [=](){ return static_cast<int>(m_scene->GetLights()[i].type); },
			[=](const int& i){ m_scene->GetLights()[i].type = static_cast<Light::Type>(i); }, groupSetting + "label=Type");

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Intensity", [=](){ return m_scene->GetLights()[i].intensity; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].intensity = v; }, groupSetting + " label=Intensity", AntTweakBarInterface::TypeHint::HDRCOLOR);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Position", [=](){ return m_scene->GetLights()[i].position; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].position = v; }, groupSetting + " label=Position", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Direction", [=](){ return m_scene->GetLights()[i].direction; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].direction = v; }, groupSetting + " label=Direction");

		m_tweakBar->AddReadWrite<float>(namePrefix + "SpotAngle", [=](){ return m_scene->GetLights()[i].halfAngle / (ei::PI / 180.0f); },
			[=](const float& f){ m_scene->GetLights()[i].halfAngle = f  * (ei::PI / 180.0f); }, groupSetting + " label=\"Half Spot Angle\" min=1.0 max=89 step=1.0");

		m_tweakBar->AddSeperator(namePrefix + "shadowseparator", groupSetting);

		m_tweakBar->AddEnum(namePrefix + "ShadowMapRes", "RSMResolution", [=](){ return m_scene->GetLights()[i].shadowMapResolution; },
			[=](const int& res){ m_scene->GetLights()[i].shadowMapResolution = res; }, groupSetting + " label=\"Shadow Map Resolution\"");

		m_tweakBar->AddReadWrite<float>(namePrefix + "NormalBias", [=](){ return m_scene->GetLights()[i].normalOffsetShadowBias; },
			[=](const float& f){ m_scene->GetLights()[i].normalOffsetShadowBias = f; }, groupSetting + " label=\"NormalOffset Shadow Bias\" min=0.00 max=100.0 step=0.001");

		m_tweakBar->AddReadWrite<float>(namePrefix + "Bias", [=](){ return m_scene->GetLights()[i].shadowBias; },
			[=](const float& f){ m_scene->GetLights()[i].shadowBias = f; }, groupSetting + " label=\"Shadow Bias\" min=0.00 max=1.0 step=0.00001");

		m_tweakBar->AddReadWrite<int>(namePrefix + "IndirectShadowLod", [=](){ return m_scene->GetLights()[i].indirectShadowComputationLod; },
			[=](const int& j){ m_scene->GetLights()[i].indirectShadowComputationLod = j; }, groupSetting + " label=\"Ind. Shadow Lod\" min=0 max=4 step=1");

		m_tweakBar->AddReadWrite<float>(namePrefix + "IndirectShadowMinConeAngle", [=](){ return m_scene->GetLights()[i].indirectShadowMinHalfConeAngle / (ei::PI / 180.0f); },
			[=](const float& f){ m_scene->GetLights()[i].indirectShadowMinHalfConeAngle = f  * (ei::PI / 180.0f); }, groupSetting + " label=\"Ind. Shadow Min. Half Angle\" min=0 max=30 step=0.1");

		m_tweakBar->SetGroupProperties(lightGroup, "Lights", lightGroup, false);
	}
}

void Application::SetupTweakBarBinding()
{
	m_tweakBar = std::make_unique<AntTweakBarInterface>(m_window->GetGLFWWindow());

	// general save/load
	{
		m_tweakBar->AddButton("Save Settings", [&](){ m_tweakBar->SaveReadWriteValuesToJSON(SaveFileDialog("settings.json", ".json")); });
		m_tweakBar->AddButton("Load Settings", [&](){ m_tweakBar->LoadReadWriteValuesToJSON(OpenFileDialog()); });
		m_tweakBar->AddButton("Save HDR Image", [&](){ std::string filename = SaveFileDialog("image.pfm", ".pfm"); if (!filename.empty()) m_renderer->SaveToPFM(filename); });
		m_tweakBar->AddSeperator("main save/load");
	}

	// Statistics
	{
		std::string statisticGroup = " group=\"Timer Statistics\"";
		m_tweakBar->AddReadOnly("TotalFrame", []{ return FrameProfiler::GetInstance().GetFrameDurations().empty() ? "" : std::to_string(FrameProfiler::GetInstance().GetFrameDurations().back() / 1000.0); }, statisticGroup);
		m_tweakBar->AddReadOnly("#Recorded", []{ return std::to_string(FrameProfiler::GetInstance().GetFrameDurations().size()); }, statisticGroup);
		m_tweakBar->AddButton("Save CSV", [&](){ std::string filename = SaveFileDialog("timestats.csv", ".csv"); if (!filename.empty()) FrameProfiler::GetInstance().SaveToCSV(filename); }, statisticGroup);
	}

	// Render mode settings
	{
		std::vector<TwEnumVal> renderModeVals =
		{
			TwEnumVal{ (int)Renderer::Mode::RSM_BRUTEFORCE, "RSM Bruteforce" },
			TwEnumVal{ (int)Renderer::Mode::DYN_RADIANCE_VOLUME, "Dyn. Cache Volume" },
			TwEnumVal{ (int)Renderer::Mode::GBUFFER_DEBUG, "GBuffer Debug" },
			TwEnumVal{ (int)Renderer::Mode::DIRECTONLY, "DirectLight only" },
			TwEnumVal{ (int)Renderer::Mode::DIRECTONLY_CACHE, "DirectLight only - via Cache" },
			TwEnumVal{ (int)Renderer::Mode::VOXELVIS, "Voxelization Display" },
			TwEnumVal{ (int)Renderer::Mode::AMBIENTOCCLUSION, "VCT AO" },
		};
		m_tweakBar->AddEnumType("RenderModeType", renderModeVals);
		m_tweakBar->AddEnum("RenderMode", "RenderModeType", [&](){ return (int)m_renderer->GetMode(); }, [&](int mode){ return m_renderer->SetMode(static_cast<Renderer::Mode>(mode)); });
		m_tweakBar->AddReadWrite<bool>("IndirectShadow", [&](){ return m_renderer->GetIndirectShadow(); }, [&](bool b){ return m_renderer->SetIndirectShadow(b); }, " label=\"Indirect Shadow\"");
		m_tweakBar->AddReadWrite<bool>("IndirectSpecular", [&](){ return m_renderer->GetIndirectSpecular(); }, [&](bool b){ return m_renderer->SetIndirectSpecular(b); }, " label=\"Indirect Specular\"");

		// Cache Settings
		m_tweakBar->AddReadWrite<int>("Address Volume Size", [&](){ return m_renderer->GetCacheAddressVolumeSize(); },
			[&](int i){ return m_renderer->SetCacheAdressVolumeSize(i); }, " min=16 max=256 step=16");
		m_tweakBar->AddReadWrite<int>("Max Total Cache Count", [&](){ return m_renderer->GetMaxCacheCount(); },
			[&](int i){ return m_renderer->SetMaxCacheCount(i); }, " min=2048 max=1048576 step=2048");

		std::vector<TwEnumVal> specEnvMapRes = { { 4, "4x4" }, { 8, "8x8" }, { 16, "16x16" }, { 32, "32x32" }, { 64, "64x64" } };
		m_tweakBar->AddEnumType("SpecEnvMapRes", specEnvMapRes);
		m_tweakBar->AddEnum("SpecEnvMap Size", "SpecEnvMapRes", [&](){ return m_renderer->GetPerCacheSpecularEnvMapSize(); }, [&](int i){ return m_renderer->SetPerCacheSpecularEnvMapSize(i); });
		m_tweakBar->AddReadWrite<int>("SpecEnvMap Hole Fill Level", [&](){ return m_renderer->GetSpecularEnvMapHoleFillLevel(); }, [&](int i){ return m_renderer->SetSpecularEnvMapHoleFillLevel(i); }, " min=0 max=5 step=1");
		m_tweakBar->AddSeperator("Render Settings");

		// Statistics
		m_tweakBar->AddReadWrite<bool>("Track Light Cache Count", [&](){ return m_renderer->GetReadLightCacheCount(); },
			[&](bool b){ return m_renderer->SetReadLightCacheCount(b); }, " group=Statistics");
		m_tweakBar->AddReadOnly("#Active Caches", [&](){ return std::to_string(m_renderer->GetLightCacheActiveCount()); }, " group=Statistics");

		m_tweakBar->AddSeperator("Rendering settings");
	}

	{
		// Camera
		m_tweakBar->AddReadWrite<float>("Camera Speed", [&](){ return m_camera->GetMoveSpeed(); }, [&](float f){ return m_camera->SetMoveSpeed(f); }, " min=0.01 max=100 step=0.01 label=Speed group=Camera");
		m_tweakBar->AddReadWrite<ei::Vec3>("Camera Direction", [&](){ return m_camera->GetDirection(); }, [&](const ei::Vec3& v){ return m_camera->SetDirection(v); }, "label=Direction group=Camera");
		m_tweakBar->AddReadWrite<ei::Vec3>("Camera Position", [&](){ return m_camera->GetPosition(); }, [&](const ei::Vec3& v){ return m_camera->SetPosition(v); }, "label=Position group=Camera", AntTweakBarInterface::TypeHint::POSITION);

		// Tonemap
		m_tweakBar->AddReadWrite<float>("Exposure", [&](){ return m_renderer->GetExposure(); }, [&](float f){ return m_renderer->SetExposure(f); }, " min=0.1 max=100 step=0.05 ");

		m_tweakBar->AddSeperator("Scene Settings");

	}

	// Entity settings
	{
		std::function<void(const int&)> changeEntityCount = std::bind(&Application::ChangeEntityCount, this, std::placeholders::_1);
		m_tweakBar->AddReadWrite<int>("Entity Count", [&](){ return static_cast<int>(m_scene->GetEntities().size()); }, changeEntityCount, " min=1 max=16 step=1 group=Entities");
	}

	// Light settings
	{
		// Define light type
		std::vector<TwEnumVal> lightTypeVals =
		{
			{ static_cast<int>(Light::Type::SPOT), "Spot" }
		};
		m_tweakBar->AddEnumType("LightType", lightTypeVals);
		// Define RSM resolution
		std::vector<TwEnumVal> rsmResVals = { { 16, "16x16" }, { 32, "32x32" }, { 64, "64x64" }, { 128, "128x128" }, { 256, "256x256" }, { 512, "512x512" } };
		m_tweakBar->AddEnumType("RSMResolution", rsmResVals);

		std::function<void(const int&)> changeLightCount = std::bind(&Application::ChangeLightCount, this, std::placeholders::_1);
		m_tweakBar->AddReadWrite<int>("Light Count", [&](){ return static_cast<int>(m_scene->GetLights().size()); }, changeLightCount, " min=1 max=16 step=1 group=Lights");
	}
}