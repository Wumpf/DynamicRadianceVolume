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
#include <glhelper/uniformbufferview.hpp>
#include <glhelper/shaderstoragebufferview.hpp>
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
	m_uboConstant = std::make_unique<gl::UniformBufferView>(*m_allShaders[0], "Constant");
	m_allShaders[0]->BindUBO(*m_uboConstant);
	m_uboPerFrame = std::make_unique<gl::UniformBufferView>(*m_allShaders[0], "PerFrame");
	m_allShaders[0]->BindUBO(*m_uboPerFrame);

	// Expecting about 16 objects. Doing triple buffering!
	m_perObjectUBOSize = m_allShaders[0]->GetUniformBufferInfo()["PerObject"].bufferDataSizeByte;
	m_perObjectUBOSize += (m_UBOAlignment - m_perObjectUBOSize % m_UBOAlignment) % m_UBOAlignment;
	unsigned int perObjectUBORingBufferSize = 16 * 3 * m_perObjectUBOSize;
	m_uboRing_PerObject = std::make_unique<gl::PersistentRingBuffer>(perObjectUBORingBufferSize);
	m_perObjectUBOBindingPoint = m_allShaders[0]->GetUniformBufferInfo()["PerObject"].bufferBinding;

	// Init specific ubos.
	m_uboDeferredDirectLighting = std::make_unique<gl::UniformBufferView>(*m_shaderDeferredDirectLighting_Spot, "Light");
	m_shaderDeferredDirectLighting_Spot->BindUBO(*m_uboDeferredDirectLighting);

	// Create voxelization module.
	m_voxelization = std::make_unique<Voxelization>(64);

	// Allocate light cache buffer
	m_maxNumLightCaches = 131072;
	const unsigned int lightCacheSizeInBytes = sizeof(float) * 4 * 6;
	m_lightCacheBuffer = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(m_maxNumLightCaches * lightCacheSizeInBytes, gl::Buffer::IMMUTABLE, nullptr), "LightCaches");
	SetReadLightCacheCount(false); // (Re)creates the lightcache buffer

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

	GL_CALL(glPatchParameteri, GL_PATCH_VERTICES, 3);
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

	m_shaderCacheInit = std::make_unique<gl::ShaderObject>("cache init");
	m_shaderCacheInit->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/cacheInit_noskinning.vert");
	m_shaderCacheInit->AddShaderFromFile(gl::ShaderObject::ShaderType::EVALUATION, "shader/cacheInit.eval");
	m_shaderCacheInit->AddShaderFromFile(gl::ShaderObject::ShaderType::CONTROL, "shader/cacheInit.cont");
	m_shaderCacheInit->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/cacheInit.frag");
	m_shaderCacheInit->CreateProgram();

	m_shaderShowCacheInitDisplay = std::make_unique<gl::ShaderObject>("cache init display");
	m_shaderShowCacheInitDisplay->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderShowCacheInitDisplay->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/cacheInitDisplay.frag");
	m_shaderShowCacheInitDisplay->CreateProgram();

	m_shaderCachePull = std::make_unique<gl::ShaderObject>("cache pull");
	m_shaderCachePull->AddShaderFromFile(gl::ShaderObject::ShaderType::COMPUTE, "shader/cachePull.comp");
	m_shaderCachePull->CreateProgram();


	m_shaderApplyCaches = std::make_unique<gl::ShaderObject>("apply caches");
	m_shaderApplyCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderApplyCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/applylightcaches.frag");
	m_shaderApplyCaches->CreateProgram();
	

	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), 
		m_shaderTonemap.get(), m_shaderCacheInit.get(), m_shaderShowCacheInitDisplay.get(), m_shaderCachePull.get(), m_shaderApplyCaches.get() };
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(it);
}

void Renderer::UpdateConstantUBO()
{
	m_uboConstant->GetBuffer()->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER);
	(*m_uboConstant)["BackbufferResolution"].Set(ei::IVec2(m_HDRBackbufferTexture->GetWidth(), m_HDRBackbufferTexture->GetHeight()));
	(*m_uboConstant)["VoxelResolution"].Set(m_voxelization->GetVoxelTexture().GetWidth());
	(*m_uboConstant)["MaxNumLightCaches"].Set(0); // TODO //static_cast<int>(m_voxelization->GetLightCacheSize()));
	m_uboConstant->GetBuffer()->Unmap();
}

void Renderer::UpdatePerFrameUBO(const Camera& camera)
{
	auto view = camera.ComputeViewMatrix();
	auto projection = camera.ComputeProjectionMatrix();
	auto viewProjection = projection * view;


	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min - 0.1f);
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max + 0.1f);

	m_uboPerFrame->GetBuffer()->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER);
	(*m_uboPerFrame)["Projection"].Set(projection);
	(*m_uboPerFrame)["ViewProjection"].Set(viewProjection);
	(*m_uboPerFrame)["InverseViewProjection"].Set(ei::invert(viewProjection));
	(*m_uboPerFrame)["CameraPosition"].Set(camera.GetPosition());
	(*m_uboPerFrame)["CameraDirection"].Set(camera.GetDirection());

	(*m_uboPerFrame)["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	(*m_uboPerFrame)["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	(*m_uboPerFrame)["VoxelSizeInWorld"].Set((voxelVolumeWorldMax - voxelVolumeWorldMin) / m_voxelization->GetVoxelTexture().GetWidth());

	m_uboPerFrame->GetBuffer()->Unmap();
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



	// Light cache setup.
	// TODO: Try Half resolution - keep in mind that this has various consequences! (depth, pull pass ...)
	m_textureCachePoints = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::RGBA16F, 1, 0);
	m_fboCachePoints.reset(new gl::FramebufferObject({ gl::FramebufferObject::Attachment(m_textureCachePoints.get()), gl::FramebufferObject::Attachment(m_GBuffer_normal.get()) },
								gl::FramebufferObject::Attachment(m_GBuffer_depth.get())));


	// CachePull compute groups are 16x16, each thread pulls 2x2 texels
	unsigned int cacheAllocationWidth = (newResolution.x + 31) / 32;
	unsigned int cacheAllocationHeight = (newResolution.y + 31) / 32;
	m_cacheAllocationMap = std::make_unique<gl::Texture2D>(cacheAllocationWidth, cacheAllocationHeight, gl::TextureFormat::RGBA8UI, 1, 0);

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

	// Fill GBuffer
	DrawSceneToGBuffer();

	// Light cache generation.
	PrepareLightCaches();

	// No more object drawing from now on.
	m_uboRing_PerObject->CompleteFrame();

	m_HDRBackbuffer->Bind(true);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	//DrawLights();
	
	ApplyLightCaches();
	
	OutputHDRTextureToBackbuffer();

//	m_voxelization->DrawVoxelRepresentation();
//	DrawGBufferDebug();

	DrawInitCacheDebug();
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

void Renderer::BindObjectUBO(unsigned int _objectIndex)
{
	m_uboRing_PerObject->BindBlockAsUBO(m_perObjectUBOBindingPoint, _objectIndex);
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

void Renderer::DrawInitCacheDebug()
{
	gl::Disable(gl::Cap::DEPTH_TEST);

	m_shaderShowCacheInitDisplay->Activate();
	gl::FramebufferObject::BindBackBuffer();

	m_textureCachePoints->Bind(0);
	m_samplerNearest.BindSampler(0);

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

	for (auto light : m_scene->GetLights())
	{
		Assert(light.type == Light::Type::SPOT, "Only spot lights are supported so far!");

		m_uboDeferredDirectLighting->GetBuffer()->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER);
		(*m_uboDeferredDirectLighting)["LightIntensity"].Set(light.intensity);
		(*m_uboDeferredDirectLighting)["LightPosition"].Set(light.position);
		(*m_uboDeferredDirectLighting)["LightDirection"].Set(ei::normalize(light.direction));
		(*m_uboDeferredDirectLighting)["LightCosHalfAngle"].Set(cosf(light.halfAngle));
		m_uboDeferredDirectLighting->GetBuffer()->Unmap();

		m_screenTriangle->Draw();
	}

	gl::Disable(gl::Cap::BLEND);
}

void Renderer::PrepareLightCaches()
{
	gl::Enable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	m_fboCachePoints->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);

	m_shaderCacheInit->Activate();
	Model::BindVAO();

	for(unsigned int entityIndex = 0; entityIndex < m_scene->GetEntities().size(); ++entityIndex)
	{
		const SceneEntity& entity = m_scene->GetEntities()[entityIndex];
		if(!entity.GetModel())
			continue;

		BindObjectUBO(entityIndex);
		entity.GetModel()->BindBuffers();
		
		GL_CALL(glDrawElements, GL_PATCHES, entity.GetModel()->GetNumTriangles() * 3, GL_UNSIGNED_INT, nullptr);
	}

	// Optionally read old light cache count
	if (m_readLightCacheCount)
	{
		const void* counterData = m_lightCacheCounter->GetBuffer()->Map(gl::Buffer::MapType::READ, gl::Buffer::MapWriteFlag::NONE);
		m_lastNumLightCaches = *reinterpret_cast<const int*>(counterData);
		m_lightCacheCounter->GetBuffer()->Unmap();
	}

	// Pull caches.
	m_lightCacheCounter->GetBuffer()->ClearToZero();

	m_cacheAllocationMap->ClearToZero();
	m_cacheAllocationMap->BindImage(0, gl::Texture::ImageAccess::WRITE);

	m_textureCachePoints->Bind(0);
	m_samplerNearest.BindSampler(0);
	m_shaderCachePull->BindSSBO(*m_lightCacheBuffer);
	m_shaderCachePull->BindSSBO(*m_lightCacheCounter);

	m_shaderCachePull->Activate();

	GL_CALL(glDispatchCompute, m_cacheAllocationMap->GetWidth(), m_cacheAllocationMap->GetHeight(), 1);
}

void Renderer::ApplyLightCaches()
{
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	m_cacheAllocationMap->Bind(3);

	//m_shaderApplyCaches->BindSSBO(*m_lightCacheBuffer);
	//m_shaderApplyCaches->BindSSBO(*m_lightCacheCounter);

	m_samplerNearest.BindSampler(0);
	m_samplerNearest.BindSampler(1);
	m_samplerNearest.BindSampler(2);
	m_samplerNearest.BindSampler(3);

	m_shaderApplyCaches->Activate();
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
	m_lightCacheCounter = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(sizeof(unsigned int), usageFlag, nullptr), "LightCacheCounter");
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