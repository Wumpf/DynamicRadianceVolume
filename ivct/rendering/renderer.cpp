#include "renderer.hpp"
#include "voxelization.hpp"

#include "../scene/model.hpp"
#include "../scene/sceneentity.hpp"
#include "../scene/scene.hpp"
#include "../camera/camera.hpp"

#include "../shaderfilewatcher.hpp"

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/screenalignedtriangle.hpp>
#include <glhelper/persistentringbuffer.hpp>
#include <glhelper/framebufferobject.hpp>
#include <glhelper/statemanagement.hpp>
#include <glhelper/utils/flagoperators.hpp>

Renderer::Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution) :
	m_samplerLinear(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Border::REPEAT))),
	m_samplerNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::REPEAT))),

	m_readLightCacheCount(false),
	m_lastNumLightCaches(0)
{
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &m_UBOAlignment);
	LOG_INFO("Uniform buffer alignment is " << m_UBOAlignment);

	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	LoadShader();

	// Init global ubos.
	m_uboInfoConstant = m_allShaders[0]->GetUniformBufferInfo()["Constant"];
	m_uboConstant = std::make_unique<gl::Buffer>(m_uboInfoConstant.bufferDataSizeByte, gl::Buffer::MAP_WRITE);
	m_allShaders[0]->BindUBO(*m_uboConstant, "Constant");

	m_uboInfoPerFrame = m_allShaders[0]->GetUniformBufferInfo()["PerFrame"];
	m_uboPerFrame = std::make_unique<gl::Buffer>(m_uboInfoPerFrame.bufferDataSizeByte, gl::Buffer::MAP_WRITE);
	m_allShaders[0]->BindUBO(*m_uboPerFrame, "PerFrame");

	// Expecting about 16 objects.
	const unsigned int maxExpectedObjects = 16;
	m_uboInfoPerObject = m_allShaders[0]->GetUniformBufferInfo()["PerObject"];
	m_uboRing_PerObject = std::make_unique<gl::PersistentRingBuffer>(maxExpectedObjects * RoundSizeToUBOAlignment(m_uboInfoPerObject.bufferDataSizeByte) * 3);

	// Light UBO.
	const unsigned int maxExpectedLights = 16;
	m_uboInfoSpotLight = m_allShaders[0]->GetUniformBufferInfo()["SpotLight"];
	m_uboRing_SpotLight = std::make_unique<gl::PersistentRingBuffer>(maxExpectedLights * RoundSizeToUBOAlignment(m_uboInfoSpotLight.bufferDataSizeByte) * 3);

	// Create voxelization module.
	m_voxelization = std::make_unique<Voxelization>(64);

	// Allocate light cache buffer
	m_maxNumLightCaches = 131072;
	const unsigned int lightCacheSizeInBytes = sizeof(float) * 4 * 6;
	m_lightCacheBuffer = std::make_unique<gl::Buffer>(m_maxNumLightCaches * lightCacheSizeInBytes, gl::Buffer::IMMUTABLE, nullptr);
	SetReadLightCacheCount(false); // (Re)creates the lightcache buffer
	// Allocate light cache hash map
	/*m_lightCacheHashMapSize = m_maxNumLightCaches * 3;
	const unsigned int lightCacheHashMapEntrySize = sizeof(unsigned int) * 2;
	m_lightCacheHashMap = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(m_lightCacheHashMapSize * lightCacheSizeInBytes, gl::Buffer::IMMUTABLE, nullptr), "LightCacheHashMap");
	*/

	const unsigned int addressGridSize = 128;
	m_lightCacheAddressVolume = std::make_unique<gl::Texture3D>(addressGridSize, addressGridSize, addressGridSize, gl::TextureFormat::R32UI, 1);

	// Basic settings.
	SetScene(scene);
	OnScreenResize(resolution);

	// General GL settings
	gl::Enable(gl::Cap::DEPTH_TEST);
	gl::Disable(gl::Cap::DITHER);

	//gl::Enable(gl::Cap::CULL_FACE);
	//GL_CALL(glFrontFace, GL_CW);

	// A quick note on depth:
	// http://www.gamedev.net/topic/568014-linear-or-non-linear-shadow-maps/#entry4633140
	// - Outputing depth manually (separate target or gl_FragDepth) can hurt performance in several ways
	// -> need to use real depthbuffer
	// --> precision issues
	// --> better precision with flipped depth test + R32F depthbuffers
	GL_CALL(glDepthFunc, GL_GREATER);
	GL_CALL(glClearDepth, 0.0f);

	// The OpenGL clip space convention uses depth -1 to 1 which is remapped again. In GL4.5 it is possible to disable this
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_ZERO_TO_ONE);
}

Renderer::~Renderer()
{
	// Unregister all shader for auto reload on change.
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(it);
}

void Renderer::LoadShader()
{
	m_shaderDebugGBuffer = std::make_unique<gl::ShaderObject>("gbuffer debug");
	m_shaderDebugGBuffer->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderDebugGBuffer->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/debuggbuffer.frag");
	m_shaderDebugGBuffer->CreateProgram();

	m_shaderFillGBuffer_noskinning = std::make_unique<gl::ShaderObject>("fill gbuffer noskinning");
	m_shaderFillGBuffer_noskinning->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/defaultmodel_noskinning.vert");
	m_shaderFillGBuffer_noskinning->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/fillgbuffer.frag");
	m_shaderFillGBuffer_noskinning->CreateProgram();

	m_shaderDeferredDirectLighting_Spot = std::make_unique<gl::ShaderObject>("direct lighting - spot");
	m_shaderDeferredDirectLighting_Spot->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderDeferredDirectLighting_Spot->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/directdeferredlighting.frag");
	m_shaderDeferredDirectLighting_Spot->CreateProgram();

	m_shaderTonemap = std::make_unique<gl::ShaderObject>("texture output");
	m_shaderTonemap->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderTonemap->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/tonemapping.frag");
	m_shaderTonemap->CreateProgram();

	m_shaderCacheGather = std::make_unique<gl::ShaderObject>("cache gather");
	m_shaderCacheGather->AddShaderFromFile(gl::ShaderObject::ShaderType::COMPUTE, "shader/cacheGather.comp");
	m_shaderCacheGather->CreateProgram();

	m_shaderCacheApply = std::make_unique<gl::ShaderObject>("apply caches");
	m_shaderCacheApply->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderCacheApply->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/cacheApply.frag");
	m_shaderCacheApply->CreateProgram();

	m_shaderLightCachesDirect = std::make_unique<gl::ShaderObject>("cache lighting direct");
	m_shaderLightCachesDirect->AddShaderFromFile(gl::ShaderObject::ShaderType::COMPUTE, "shader/cacheLightingDirect.comp");
	m_shaderLightCachesDirect->CreateProgram();


	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), 
						m_shaderTonemap.get(), m_shaderCacheGather.get(), m_shaderCacheApply.get(), m_shaderLightCachesDirect.get() };
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(it);
}

void Renderer::UpdateConstantUBO()
{
	gl::MappedUBOView mappedMemory(m_uboInfoConstant, m_uboConstant->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER));
	mappedMemory["BackbufferResolution"].Set(ei::IVec2(m_HDRBackbufferTexture->GetWidth(), m_HDRBackbufferTexture->GetHeight()));
	mappedMemory["VoxelResolution"].Set(m_voxelization->GetVoxelTexture().GetWidth());
	mappedMemory["MaxNumLightCaches"].Set(m_maxNumLightCaches);
	m_uboConstant->Unmap();
}

void Renderer::UpdatePerFrameUBO(const Camera& camera)
{
	auto view = camera.ComputeViewMatrix();
	auto projection = camera.ComputeProjectionMatrix();
	auto viewProjection = projection * view;


	ei::Vec3 VolumeWorldMin(m_scene->GetBoundingBox().min - 0.1f);
	ei::Vec3 VolumeWorldMax(m_scene->GetBoundingBox().max + 0.1f);

	gl::MappedUBOView mappedMemory(m_allShaders[0]->GetUniformBufferInfo()["PerFrame"], m_uboPerFrame->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER));
	mappedMemory["Projection"].Set(projection);
	mappedMemory["ViewProjection"].Set(viewProjection);
	mappedMemory["InverseViewProjection"].Set(ei::invert(viewProjection));
	mappedMemory["CameraPosition"].Set(camera.GetPosition());
	mappedMemory["CameraDirection"].Set(camera.GetDirection());

	mappedMemory["VolumeWorldMin"].Set(VolumeWorldMin);
	mappedMemory["VolumeWorldMax"].Set(VolumeWorldMax);
	mappedMemory["VoxelSizeInWorld"].Set((VolumeWorldMax - VolumeWorldMin) / m_voxelization->GetVoxelTexture().GetWidth());
	mappedMemory["AddressVolumeVoxelSize"].Set((VolumeWorldMax - VolumeWorldMin) / m_lightCacheAddressVolume->GetWidth());

	m_uboPerFrame->Unmap();
}

void Renderer::OnScreenResize(const ei::UVec2& newResolution)
{
	m_GBuffer_diffuse = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::SRGB8_ALPHA8, 1, 0);
	m_GBuffer_normal = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::RG16I, 1, 0);
	m_GBuffer_depth = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::DEPTH_COMPONENT32F, 1, 0);

	// Render to snorm integer makes problems.
	// Others seem to have this problem too http://www.gamedev.net/topic/657167-opengl-44-render-to-snorm/

	m_GBuffer.reset(new gl::FramebufferObject({ gl::FramebufferObject::Attachment(m_GBuffer_diffuse.get()), gl::FramebufferObject::Attachment(m_GBuffer_normal.get()) },
									gl::FramebufferObject::Attachment(m_GBuffer_depth.get())));


	m_HDRBackbufferTexture = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::RGBA16F, 1, 0);
	m_HDRBackbuffer.reset(new gl::FramebufferObject(gl::FramebufferObject::Attachment(m_HDRBackbufferTexture.get())));

	GL_CALL(glViewport, 0, 0, newResolution.x, newResolution.y);


	UpdateConstantUBO();
}

void Renderer::SetScene(const std::shared_ptr<const Scene>& scene)
{
	Assert(scene.get(), "Scene pointer is null!");

	m_scene = scene;

	if(m_HDRBackbufferTexture)
		UpdateConstantUBO();
}

void Renderer::Draw(const Camera& camera)
{
	// Update data.
	UpdatePerFrameUBO(camera);
	UpdatePerObjectUBORingBuffer();
	PrepareLights();

	// Scene dependent renderings.
	DrawSceneToGBuffer();
	DrawShadowMaps();

	// Light cache generation.
	//GatherLightCaches();

	// No more object drawing from now on.
	m_uboRing_PerObject->CompleteFrame();

	m_HDRBackbuffer->Bind(true);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	DrawLights();

	//DirectCacheLighting();

	// No light data needed from now on.
	m_uboRing_SpotLight->CompleteFrame();
	
	//ApplyLightCaches();
	
	OutputHDRTextureToBackbuffer();

//	m_voxelization->DrawVoxelRepresentation();
//	DrawGBufferDebug();
}

void Renderer::UpdatePerObjectUBORingBuffer()
{
	for (unsigned int entityIndex = 0; entityIndex < m_scene->GetEntities().size(); ++entityIndex)
	{
		const SceneEntity& entity = m_scene->GetEntities()[entityIndex];
		
		void* blockMemory = nullptr;
		size_t blockIndex = 0;	
		m_uboRing_PerObject->AddBlock(blockMemory, blockIndex, sizeof(ei::Mat4x4), m_UBOAlignment);
		Assert(blockIndex == entityIndex, "Entity index and memory block index are different.");

		auto worldMatrix = entity.ComputeWorldMatrix();
		memcpy(blockMemory, &worldMatrix, sizeof(ei::Mat4x4));
	}
	m_uboRing_PerObject->FlushAllBlocks();
}

void Renderer::PrepareLights()
{
	for (unsigned int lightIndex = 0; lightIndex < m_scene->GetLights().size(); ++lightIndex)
	{
		const Light& light = m_scene->GetLights()[lightIndex];
		Assert(light.type == Light::Type::SPOT, "Only spot lights are supported so far!");


		void* blockMemory = nullptr;
		size_t blockIndex = 0;
		m_uboRing_SpotLight->AddBlock(blockMemory, blockIndex, m_uboInfoSpotLight.bufferDataSizeByte, m_UBOAlignment);
		Assert(blockIndex == lightIndex, "Light index and memory block index are different.");

		gl::MappedUBOView uboView(m_uboInfoSpotLight, blockMemory);
		uboView["LightIntensity"].Set(light.intensity);
		uboView["LightPosition"].Set(light.position);
		uboView["LightDirection"].Set(ei::normalize(light.direction));
		uboView["LightCosHalfAngle"].Set(cosf(light.halfAngle));
	}
}

void Renderer::BindObjectUBO(unsigned int _objectIndex)
{
	m_uboRing_PerObject->BindBlockAsUBO(m_uboInfoPerObject.bufferBinding, _objectIndex);
}

void Renderer::OutputHDRTextureToBackbuffer()
{
	gl::FramebufferObject::BindBackBuffer();
	gl::Enable(gl::Cap::FRAMEBUFFER_SRGB);
	m_shaderTonemap->Activate();

	m_samplerNearest.BindSampler(0);
	m_HDRBackbufferTexture->Bind(0);

	m_screenTriangle->Draw();
	gl::Disable(gl::Cap::FRAMEBUFFER_SRGB);
}

void Renderer::DrawSceneToGBuffer()
{
	gl::Enable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);

	m_samplerLinear.BindSampler(0);

	m_shaderFillGBuffer_noskinning->Activate();
	m_GBuffer->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene(true);
}

void Renderer::DrawShadowMaps()
{

}

void Renderer::DrawGBufferDebug()
{
	gl::Disable(gl::Cap::DEPTH_TEST);

	m_shaderDebugGBuffer->Activate();
	gl::FramebufferObject::BindBackBuffer();

	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);

	m_samplerNearest.BindSampler(0);
	m_samplerNearest.BindSampler(1);
	m_samplerNearest.BindSampler(2);
	m_samplerNearest.BindSampler(3);

	m_screenTriangle->Draw();
}

void Renderer::DrawLights()
{
	gl::Disable(gl::Cap::DEPTH_TEST);
	gl::Enable(gl::Cap::BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	m_shaderDeferredDirectLighting_Spot->Activate();

	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);


	for (unsigned int lightIndex = 0; lightIndex < m_scene->GetLights().size(); ++lightIndex)
	{
		m_uboRing_SpotLight->BindBlockAsUBO(m_uboInfoSpotLight.bufferBinding, lightIndex);
		m_screenTriangle->Draw();
	}

	gl::Disable(gl::Cap::BLEND);
}

void Renderer::DirectCacheLighting()
{
	GL_CALL(glBindBuffer, GL_DISPATCH_INDIRECT_BUFFER, m_lightCacheCounter->GetInternHandle());
	m_shaderLightCachesDirect->BindSSBO(*m_lightCacheBuffer, "LightCacheBuffer");
	m_shaderLightCachesDirect->BindSSBO(*m_lightCacheCounter, "LightCacheCounter");

	m_shaderLightCachesDirect->Activate();

	GL_CALL(glMemoryBarrier, GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

	auto& spotuboInfo = m_shaderLightCachesDirect->GetUniformBufferInfo()["SpotLight"];

	for (unsigned int lightIndex = 0; lightIndex < m_scene->GetLights().size(); ++lightIndex)
	{
		m_uboRing_SpotLight->BindBlockAsUBO(m_uboInfoSpotLight.bufferBinding, lightIndex);
		GL_CALL(glDispatchComputeIndirect, 0);
	}
}

void Renderer::GatherLightCaches()
{
	gl::Disable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	// Optionally read old light cache count
	if (m_readLightCacheCount)
	{
		const void* counterData = m_lightCacheCounter->Map(gl::Buffer::MapType::READ, gl::Buffer::MapWriteFlag::NONE);
		m_lastNumLightCaches = reinterpret_cast<const int*>(counterData)[3];
		m_lightCacheCounter->Unmap();
	}

	m_lightCacheCounter->ClearToZero();
	m_lightCacheBuffer->ClearToZero();
	//m_lightCacheHashMap->GetBuffer()->ClearToZero();
	m_lightCacheAddressVolume->ClearToZero();


	m_shaderCacheGather->BindSSBO(*m_lightCacheCounter, "LightCacheBuffer");
	m_shaderCacheGather->BindSSBO(*m_lightCacheBuffer, "LightCacheCounter");
	//m_shaderCacheGather->BindSSBO(*m_lightCacheHashMap);
	m_lightCacheAddressVolume->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);

	m_shaderCacheGather->Activate();

	const unsigned int threadsPerGroupX = 16;
	const unsigned int threadsPerGroupY = 16;
	unsigned numThreadGroupsX = (m_HDRBackbufferTexture->GetWidth() + threadsPerGroupX - 1) / threadsPerGroupX;
	unsigned numThreadGroupsY = (m_HDRBackbufferTexture->GetHeight() + threadsPerGroupY - 1) / threadsPerGroupY;
	GL_CALL(glDispatchCompute, numThreadGroupsX, numThreadGroupsY, 1);
}

void Renderer::ApplyLightCaches()
{
	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	m_samplerNearest.BindSampler(0);
	m_samplerNearest.BindSampler(1);
	m_samplerNearest.BindSampler(2);

	//m_shaderCacheApply->BindSSBO(*m_lightCacheHashMap);
	m_shaderCacheApply->BindSSBO(*m_lightCacheBuffer, "LightCacheBuffer");

	m_lightCacheAddressVolume->Bind(3);
	m_samplerNearest.BindSampler(3);

	m_shaderCacheApply->Activate();

	GL_CALL(glMemoryBarrier, GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	m_screenTriangle->Draw();
}

void Renderer::DrawScene(bool setTextures)
{
	Model::BindVAO();

	for (unsigned int entityIndex =0; entityIndex < m_scene->GetEntities().size(); ++entityIndex)
	{
		const SceneEntity& entity = m_scene->GetEntities()[entityIndex];
		if (!entity.GetModel())
			continue;

		BindObjectUBO(entityIndex);
		entity.GetModel()->BindBuffers();
		for (const Model::Mesh& mesh : entity.GetModel()->GetMeshes())
		{
			if (setTextures)
			{
				if (mesh.diffuseTexture)
					mesh.diffuseTexture->Bind(0);
				else
					gl::Texture2D::ResetBinding(0);
			}
			GL_CALL(glDrawElements, GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(sizeof(std::uint32_t) * mesh.startIndex));
		}
	}
}

void Renderer::SetReadLightCacheCount(bool trackLightCacheHashCollisionCount)
{
	m_readLightCacheCount = trackLightCacheHashCollisionCount;
	gl::Buffer::UsageFlag usageFlag = gl::Buffer::IMMUTABLE;
	if (trackLightCacheHashCollisionCount)
		usageFlag = gl::Buffer::MAP_READ;
	m_lightCacheCounter = std::make_unique<gl::Buffer>(sizeof(unsigned int) * 4, usageFlag, nullptr);
	m_lastNumLightCaches = 0;
}

bool Renderer::GetReadLightCacheCount() const
{
	return m_readLightCacheCount;
}

unsigned int Renderer::GetLightCacheActiveCount() const
{
	return m_lastNumLightCaches;
}

void Renderer::ShadowMap::Init(unsigned int resolution)
{

}