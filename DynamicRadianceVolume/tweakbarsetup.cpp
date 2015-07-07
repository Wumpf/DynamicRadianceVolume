#include "application.hpp"

#include "outputwindow.hpp"

#include "rendering/renderer.hpp"
#include "rendering/frustumoutlines.hpp"

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
	m_tweakBar->SetGroupProperties("Entities", "", "Entities", false);
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
			[=](int i){ m_scene->GetLights()[i].type = static_cast<Light::Type>(i); }, groupSetting + "label=Type");

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Intensity", [=](){ return m_scene->GetLights()[i].intensity; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].intensity = v; }, groupSetting + " label=Intensity", AntTweakBarInterface::TypeHint::HDRCOLOR);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Position", [=](){ return m_scene->GetLights()[i].position; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].position = v; }, groupSetting + " label=Position", AntTweakBarInterface::TypeHint::POSITION);

		m_tweakBar->AddReadWrite<ei::Vec3>(namePrefix + "Direction", [=](){ return m_scene->GetLights()[i].direction; },
			[=](const ei::Vec3& v){ m_scene->GetLights()[i].direction = v; }, groupSetting + " label=Direction");

		m_tweakBar->AddReadWrite<float>(namePrefix + "SpotAngle", [=](){ return m_scene->GetLights()[i].halfAngle / (ei::PI / 180.0f); },
			[=](float f){ m_scene->GetLights()[i].halfAngle = f  * (ei::PI / 180.0f); }, groupSetting + " label=\"Half Spot Angle\" min=1.0 max=89 step=1.0");

		m_tweakBar->AddSeperator(namePrefix + "shadowseparator", groupSetting);

		m_tweakBar->AddEnum(namePrefix + "RSMReadResolution", "RSMResolution", [=](){ return static_cast<unsigned int>(m_scene->GetLights()[i].rsmResolution / pow(2, m_scene->GetLights()[i].rsmReadLod)); },
			[=](int res){ m_scene->GetLights()[i].rsmReadLod = static_cast<unsigned int>(log2(m_scene->GetLights()[i].rsmResolution / res)); }, groupSetting + " label=\"RSM Read Resolution\"");

		m_tweakBar->AddEnum(namePrefix + "RSMRenderResolution", "RSMResolution", [=](){ return m_scene->GetLights()[i].rsmResolution; },
			[=](int res){ 
				int readRes = static_cast<int>(m_scene->GetLights()[i].rsmResolution / pow(2, m_scene->GetLights()[i].rsmReadLod));
				m_scene->GetLights()[i].rsmResolution = res;
				m_scene->GetLights()[i].rsmReadLod = static_cast<unsigned int>(log2(res / readRes));
			}, groupSetting + " label=\"RSM Render Resolution\"");

		m_tweakBar->AddReadWrite<float>(namePrefix + "NormalBias", [=](){ return m_scene->GetLights()[i].normalOffsetShadowBias; },
			[=](float f){ m_scene->GetLights()[i].normalOffsetShadowBias = f; }, groupSetting + " label=\"NormalOffset Shadow Bias\" min=0.00 max=100.0 step=0.001");

		m_tweakBar->AddReadWrite<float>(namePrefix + "Bias", [=](){ return m_scene->GetLights()[i].shadowBias; },
			[=](float f){ m_scene->GetLights()[i].shadowBias = f; }, groupSetting + " label=\"Shadow Bias\" min=0.00 max=1.0 step=0.00001");

		m_tweakBar->AddReadWrite<int>(namePrefix + "IndirectShadowLod", [=](){ return m_scene->GetLights()[i].indirectShadowComputationLod; },
			[=](int j){ m_scene->GetLights()[i].indirectShadowComputationLod = j; }, groupSetting + " label=\"Ind. Shadow Lod\" min=0 max=4 step=1");

		m_tweakBar->SetGroupProperties(lightGroup, "Lights", lightGroup, false);
	}
	m_tweakBar->SetGroupProperties("Lights", "", "Lights", false);
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

	// Render mode settings
	{
		std::vector<TwEnumVal> renderModeVals =
		{
			TwEnumVal{ (int)Renderer::Mode::RSM_BRUTEFORCE, "RSM Bruteforce (slow!)" },
			TwEnumVal{ (int)Renderer::Mode::DYN_RADIANCE_VOLUME, "Dyn. Cache Volume" },
			TwEnumVal{ (int)Renderer::Mode::DYN_RADIANCE_VOLUME_DEBUG, "Dyn. Cache Volume Debug" },
			TwEnumVal{ (int)Renderer::Mode::GBUFFER_DEBUG, "GBuffer Debug" },
			TwEnumVal{ (int)Renderer::Mode::DIRECTONLY, "DirectLight only" },
			//TwEnumVal{ (int)Renderer::Mode::DIRECTONLY_CACHE, "DirectLight only - via Cache" },
			TwEnumVal{ (int)Renderer::Mode::VOXELVIS, "Voxelization Display" },
			TwEnumVal{ (int)Renderer::Mode::AMBIENTOCCLUSION, "VCT AO" },
		};
		m_tweakBar->AddEnumType("RenderModeType", renderModeVals);
		m_tweakBar->AddEnum("Render Mode", "RenderModeType", [&](){ return (int)m_renderer->GetMode(); }, [&](int mode){ return m_renderer->SetMode(static_cast<Renderer::Mode>(mode)); });
		m_tweakBar->AddReadWrite<bool>("IndirectShadow", [&](){ return m_renderer->GetIndirectShadow(); }, [&](bool b){ return m_renderer->SetIndirectShadow(b); }, " label=\"Indirect Shadow\"");
		m_tweakBar->AddReadWrite<bool>("IndirectSpecular", [&](){ return m_renderer->GetIndirectSpecular(); }, [&](bool b){ return m_renderer->SetIndirectSpecular(b); }, " label=\"Indirect Specular\"");

		std::vector<TwEnumVal> indirectDiffuseModeVals =
		{
			TwEnumVal{ (int)Renderer::IndirectDiffuseMode::SH1, "0/1 Band SH" },
			TwEnumVal{ (int)Renderer::IndirectDiffuseMode::SH2, "0/1/2 Band SH" },
		};
		m_tweakBar->AddEnumType("IndirectDiffuseModeType", indirectDiffuseModeVals);
		m_tweakBar->AddEnum("Indirect Diffuse Mode", "IndirectDiffuseModeType", [&](){ return (int)m_renderer->GetIndirectDiffuseMode(); }, [&](int mode){ return m_renderer->SetIndirectDiffuseMode(static_cast<Renderer::IndirectDiffuseMode>(mode)); });

		m_tweakBar->AddReadWrite<int>("Voxel Resolution", [&](){ return m_renderer->GetVoxelVolumeResultion(); }, [&](int i){ return m_renderer->SetVoxelVolumeResultion(i); }, " min=16 max=512 step=16");
		m_tweakBar->AddReadWrite<float>("Voxel Refresh Interval", [&](){ return m_renderer->GetVoxelVolumeRefreshInterval(); }, [&](float f){ return m_renderer->SetVoxelVolumeRefreshInterval(f); }, " min=0.0 max=1.0 step=0.05");

		m_tweakBar->AddReadWrite<int>("Max Total Cache Count", [&](){ return m_renderer->GetMaxCacheCount(); },
			[&](int i){ return m_renderer->SetMaxCacheCount(i); }, " min=2048 max=1048576 step=2048");
		m_tweakBar->AddReadWrite<bool>("Track Light Cache Count", [&](){ return m_renderer->GetReadLightCacheCount(); },
			[&](bool b){ return m_renderer->SetReadLightCacheCount(b); });
		m_tweakBar->AddReadOnly("#Active Caches", [&](){ return std::to_string(m_renderer->GetLightCacheActiveCount()); });

		// Address Volume
		std::string groupSetting = " group=AddressVolume";
		m_tweakBar->AddReadWrite<float>("Transition Size", [&](){ return m_renderer->GetCAVCascadeTransitionSize(); }, [&](float f){ return m_renderer->SetCAVCascadeTransitionSize(f); }, groupSetting + " min=0.0 max=10.0 step=0.1");
		m_tweakBar->AddReadWrite<bool>("Display Cascades", [&](){ return m_renderer->GetShowCAVCascades(); }, [&](bool b){ return m_renderer->SetShowCAVCascades(b); }, groupSetting);
		m_tweakBar->AddReadWrite<int>("Resolution", [&](){ return m_renderer->GetCAVResolution(); },
			[&](int i){ return m_renderer->SetCAVCascades(m_renderer->GetCAVCascadeCount(), i); }, " min=16 max=256 step=16" + groupSetting);
		m_tweakBar->AddReadWrite<int>("#Cascades", [&](){ return m_renderer->GetCAVCascadeCount(); },
			[&](int numCascades)
			{
				for (int i = 0; i < Renderer::s_maxNumCAVCascades; ++i)
					m_tweakBar->SetVisible("CascadeSize_" + std::to_string(i), i < numCascades);
				return m_renderer->SetCAVCascades(numCascades, m_renderer->GetCAVResolution());
			}, " min=1 max=4 step=1" + groupSetting);
		for (unsigned int i = 0; i < Renderer::s_maxNumCAVCascades; ++i)
		{
			std::string elementSettings = " min=1.0 max=10000 step=0.5" + groupSetting + " label=\"Cascade Size " + std::to_string(i) + "\"";
			if (i < m_renderer->GetCAVCascadeCount()) elementSettings += " visible=false";
			m_tweakBar->AddReadWrite<float>("CascadeSize_" + std::to_string(i), [&, i](){ return m_renderer->GetCAVCascadeWorldSize(i); },
				[&, i](float size){ m_renderer->SetCAVCascadeWorldSize(i, size); }, elementSettings);
		}
		m_tweakBar->SetGroupProperties("AddressVolume", "", "Light Cache Address Volume (CAV)", false);

		
		// Address Volume
		groupSetting = " group=SpecEnvMap";
		std::vector<TwEnumVal> specEnvMapRes = { { 4, "4x4" }, { 8, "8x8" }, { 16, "16x16" }, { 32, "32x32" }, { 64, "64x64" } };
		m_tweakBar->AddEnumType("SpecEnvMapRes", specEnvMapRes);
		m_tweakBar->AddEnum("SpecEnvMap Size", "SpecEnvMapRes", [&](){ return m_renderer->GetPerCacheSpecularEnvMapSize(); }, [&](int i){ return m_renderer->SetPerCacheSpecularEnvMapSize(i); }, " label=Resolution" + groupSetting);
		m_tweakBar->AddReadWrite<int>("Hole Fill Level", [&](){ return m_renderer->GetSpecularEnvMapHoleFillLevel(); }, [&](int i){ return m_renderer->SetSpecularEnvMapHoleFillLevel(i); }, " min=0 max=5 step=1" + groupSetting);
		m_tweakBar->AddReadWrite<bool>("Direct Write", [&](){ return m_renderer->GetSpecularEnvMapDirectWrite(); }, [&](bool b){ return m_renderer->SetSpecularEnvMapDirectWrite(b); }, groupSetting);
		m_tweakBar->SetGroupProperties("SpecEnvMap", "", "Indirect Specular Settings", false);
	}

	// Timer Statistics
	{
		m_tweakBar->AddButton("Clear stats", [](){ FrameProfiler::GetInstance().WaitForQueryResults();  FrameProfiler::GetInstance().Clear(); }, m_tweakBarStatisticGroupSetting);
		m_tweakBar->AddButton("Save CSV", [&](){ std::string filename = SaveFileDialog("timestats.csv", ".csv"); if (!filename.empty()) FrameProfiler::GetInstance().SaveToCSV(filename); }, m_tweakBarStatisticGroupSetting);
		m_tweakBar->AddReadWrite<bool>("GPU Queries Active", []{ return FrameProfiler::GetInstance().GetGPUProfilingActive(); }, [](bool active){ FrameProfiler::GetInstance().SetGPUProfilingActive(active); }, m_tweakBarStatisticGroupSetting);

		m_tweakBar->AddReadOnly("Total Frame (µs)", []{ return FrameProfiler::GetInstance().GetFrameDurations().empty() ? "" : std::to_string(FrameProfiler::GetInstance().GetFrameDurations().back() / 1000.0); }, m_tweakBarStatisticGroupSetting);
		m_tweakBar->AddReadOnly("#Recorded", []{ return std::to_string(FrameProfiler::GetInstance().GetFrameDurations().size()); }, m_tweakBarStatisticGroupSetting);
		m_tweakBar->AddSeperator("GPU Queries:", m_tweakBarStatisticGroupSetting);

		m_tweakBar->SetGroupProperties(m_tweakBarStatisticGroupSetting.substr(m_tweakBarStatisticGroupSetting.find('=')+1), "", "Timer Stats", false);
	}

	m_tweakBar->AddSeperator("General Render Mode Settings");

	{
		// Camera
		m_tweakBar->AddReadWrite<bool>("Freeze Dependent Updates", [&](){ return m_detachViewFromCameraUpdate; }, [&](bool b){ m_detachViewFromCameraUpdate = b; if (b) m_frustumOutlineRenderer->Update(*m_camera); }, " group=Camera");
		m_tweakBar->AddReadWrite<float>("Camera Speed", [&](){ return m_camera->GetMoveSpeed(); }, [&](float f){ m_camera->SetMoveSpeed(f); }, " min=0.01 max=100 step=0.01 label=Speed group=Camera");
		m_tweakBar->AddReadWrite<ei::Vec3>("Camera Direction", [&](){ return m_camera->GetDirection(); }, [&](const ei::Vec3& v){ m_camera->SetDirection(v); }, "label=Direction group=Camera");
		m_tweakBar->AddReadWrite<ei::Vec3>("Camera Position", [&](){ return m_camera->GetPosition(); }, [&](const ei::Vec3& v){ m_camera->SetPosition(v); }, "label=Position group=Camera", AntTweakBarInterface::TypeHint::POSITION);

		// Tonemap
		m_tweakBar->AddReadWrite<float>("Exposure", [&](){ return m_renderer->GetExposure(); }, [&](float f){ return m_renderer->SetExposure(f); }, " min=0.1 max=100 step=0.05 ");
		m_tweakBar->AddReadWrite<float>("L Max", [&](){ return m_renderer->GetTonemapLMax(); }, [&](float f){ return m_renderer->SetTonemapLMax(f); }, " min=0.1 max=100 step=0.05 ");

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
		std::vector<TwEnumVal> rsmResVals = { { 16, "16x16" }, { 32, "32x32" }, { 64, "64x64" }, { 128, "128x128" }, { 256, "256x256" }, { 512, "512x512" }, { 1024, "1024x1024" }, { 2048, "2048x2048" } };
		m_tweakBar->AddEnumType("RSMResolution", rsmResVals);

		std::function<void(const int&)> changeLightCount = std::bind(&Application::ChangeLightCount, this, std::placeholders::_1);
		m_tweakBar->AddReadWrite<int>("Light Count", [&](){ return static_cast<int>(m_scene->GetLights().size()); }, changeLightCount, " min=1 max=16 step=1 group=Lights");
	}
}