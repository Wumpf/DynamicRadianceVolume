#include "renderer.hpp"
#include "voxelization.hpp"

#include "../scene/model.hpp"
#include "../scene/scene.hpp"
#include "../camera/camera.hpp"

#include "../shaderfilewatcher.hpp"

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/screenalignedtriangle.hpp>
#include <glhelper/uniformbufferview.hpp>
#include <glhelper/shaderstoragebufferview.hpp>
#include <glhelper/framebufferobject.hpp>
#include <glhelper/statemanagement.hpp>
#include <glhelper/utils/flagoperators.hpp>


Renderer::Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution) :
	m_samplerLinear(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Border::REPEAT))),
	m_samplerNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::REPEAT))),

	m_trackLightCacheHashCollisionCount(false),
	m_lastLightCacheHashCollisionCount(0)
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	LoadShader();

	// Init global ubos.
	m_uboConstant = std::make_unique<gl::UniformBufferView>(*m_allShaders[0], "Constant");
	m_allShaders[0]->BindUBO(*m_uboConstant);
	m_uboPerFrame = std::make_unique<gl::UniformBufferView>(*m_allShaders[0], "PerFrame");
	m_allShaders[0]->BindUBO(*m_uboPerFrame);

	// Init specific ubos.
	m_uboDeferredDirectLighting = std::make_unique<gl::UniformBufferView>(*m_shaderDeferredDirectLighting_Spot, "Light");
	m_shaderDeferredDirectLighting_Spot->BindUBO(*m_uboDeferredDirectLighting);

	// Create voxelization module.
	m_voxelization = std::make_unique<Voxelization>(ei::UVec3(256, 256, 256));

	// Basic settings.
	SetScene(scene);
	OnScreenResize(resolution);

	// misc buffer
	m_lightCacheHashCollisionCounter = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(sizeof(std::uint32_t), gl::Buffer::Usage::MAP_READ | gl::Buffer::Usage::MAP_WRITE), "LightCacheHashCollisionCounter");


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

	m_shaderFillLightCaches = std::make_unique<gl::ShaderObject>("fill light caches");
	m_shaderFillLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	std::string defines = m_trackLightCacheHashCollisionCount ? "#define COUNT_LIGHTCACHE_HASH_COLLISIONS" : "";
	m_shaderFillLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/filllightcaches.frag", defines);
	m_shaderFillLightCaches->CreateProgram();


	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), m_shaderTonemap.get(), m_shaderFillLightCaches.get() };
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(it);

}

void Renderer::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min);
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max);
	ei::Vec3 voxelVolumeSizePix(static_cast<float>(m_voxelization->GetVoxelTexture().GetWidth()), 
								static_cast<float>(m_voxelization->GetVoxelTexture().GetHeight()), 
								static_cast<float>(m_voxelization->GetVoxelTexture().GetDepth()));
	

	m_uboConstant->GetBuffer()->Map();
	(*m_uboConstant)["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	(*m_uboConstant)["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	(*m_uboConstant)["VoxelSizeInWorld"].Set((voxelVolumeWorldMax - voxelVolumeWorldMin) / voxelVolumeSizePix);

	m_cacheWorldSize = ei::Vec3(5.0f); // TODO
	(*m_uboConstant)["CacheWorldSize"].Set(m_cacheWorldSize);
	m_uboConstant->GetBuffer()->Unmap();
}

void Renderer::UpdatePerFrameUBO(const Camera& camera)
{
	auto view = camera.ComputeViewMatrix();
	auto projection = camera.ComputeProjectionMatrix();
	auto viewProjection = projection * view;


	m_uboPerFrame->GetBuffer()->Map();
	(*m_uboPerFrame)["ViewProjection"].Set(viewProjection);
	(*m_uboPerFrame)["InverseViewProjection"].Set(ei::invert(viewProjection));
	(*m_uboPerFrame)["CameraPosition"].Set(camera.GetPosition());

	ei::Vec3 cacheGridMin(ei::floor(m_scene->GetBoundingBox().min) - ei::Vec3(0.5f));
	ei::Vec3 cacheGridMax(ei::ceil(m_scene->GetBoundingBox().max) + ei::Vec3(0.5f));

	(*m_uboPerFrame)["CacheGridMin"].Set(cacheGridMin); // TODO: Better box - union of camera and scene box
	ei::Vec3 cacheGridSize = cacheGridMax - cacheGridMin;
	ei::Vec3 cacheGridCellCount = cacheGridSize / m_cacheWorldSize;
	Assert(cacheGridCellCount.x * cacheGridCellCount.y * cacheGridCellCount.z < std::numeric_limits<int>().max(), "Too many virtual light cache entries!");

	(*m_uboPerFrame)["CacheGridStrideX"].Set(static_cast<std::int32_t>(cacheGridCellCount.x));
	(*m_uboPerFrame)["CacheGridSize"].Set(cacheGridSize);
	(*m_uboPerFrame)["CacheGridStrideXY"].Set(static_cast<std::int32_t>(cacheGridCellCount.x) * static_cast<std::int32_t>(cacheGridCellCount.y));
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
	static const std::int32_t lightCacheEntrySize = sizeof(float) * 4 * 1;
	std::int32_t maxNumLightCaches = (newResolution.x/2) * (newResolution.y/2);
	m_lightCaches = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(lightCacheEntrySize * maxNumLightCaches, gl::Buffer::Usage::IMMUTABLE), "LightCaches");
	
	m_texturePerPixelCacheEntries = std::make_unique<gl::Texture2D>(newResolution.x, newResolution.y, gl::TextureFormat::R32I, 1, 0);
	m_fboPerPixelCacheEntries = std::make_unique<gl::FramebufferObject>(gl::FramebufferObject::Attachment(m_texturePerPixelCacheEntries.get()));

	m_uboConstant->GetBuffer()->Map();//(*m_uboConstant)["MaxNumLightCaches"].GetMetaInfo().blockOffset, 4);
	(*m_uboConstant)["MaxNumLightCaches"].Set(maxNumLightCaches);
	m_uboConstant->GetBuffer()->Unmap();
}

void Renderer::SetScene(const std::shared_ptr<const Scene>& scene)
{
	Assert(scene.get(), "Scene pointer is null!");

	m_scene = scene;

	UpdateConstantUBO();
}

void Renderer::Draw(const Camera& camera)
{
	UpdatePerFrameUBO(camera);

	m_voxelization->VoxelizeScene(*m_scene);
	GL_CALL(glViewport, 0, 0, m_HDRBackbufferTexture->GetWidth(), m_HDRBackbufferTexture->GetHeight());
	m_voxelization->DrawVoxelRepresentation();

	//DrawSceneToGBuffer();
	//FillLightCaches();

	//m_HDRBackbuffer->Bind(false);
	//GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	//DrawLights();
	//DrawGBufferDebug();

	//OutputHDRTextureToBackbuffer();
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

void Renderer::FillLightCaches()
{
	if (m_trackLightCacheHashCollisionCount)
	{
		std::uint32_t* hashCollisionCount = static_cast<std::uint32_t*>(m_lightCacheHashCollisionCounter->GetBuffer()->Map());
		m_lastLightCacheHashCollisionCount = *hashCollisionCount;
		*hashCollisionCount = 0;
		m_lightCacheHashCollisionCounter->GetBuffer()->Unmap();

		m_shaderFillLightCaches->BindSSBO(*m_lightCacheHashCollisionCounter);
	}

	m_lightCaches->GetBuffer()->ClearToZero();
	m_shaderFillLightCaches->BindSSBO(*m_lightCaches);

	m_fboPerPixelCacheEntries->Bind(false);

	m_shaderFillLightCaches->Activate();
	m_screenTriangle->Draw();
}

void Renderer::DrawSceneToGBuffer()
{
	gl::Enable(gl::Cap::DEPTH_TEST);

	m_samplerLinear.BindSampler(0);

	m_shaderFillGBuffer_noskinning->Activate();
	m_GBuffer->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene();
}

void Renderer::DrawGBufferDebug()
{
	gl::Disable(gl::Cap::DEPTH_TEST);

	m_shaderDebugGBuffer->Activate();
	gl::FramebufferObject::BindBackBuffer();

	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	m_texturePerPixelCacheEntries->Bind(3);

	// For accessing light cache buffer (m_lightCaches)
	GL_CALL(glMemoryBarrier, GL_SHADER_STORAGE_BARRIER_BIT);

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

	for (auto light : m_scene->GetLights())
	{
		Assert(light.type == Light::Type::SPOT, "Only spot lights are supported so far!");

		m_uboDeferredDirectLighting->GetBuffer()->Map();
		(*m_uboDeferredDirectLighting)["LightIntensity"].Set(light.intensity);
		(*m_uboDeferredDirectLighting)["LightPosition"].Set(light.position);
		(*m_uboDeferredDirectLighting)["LightDirection"].Set(ei::normalize(light.direction));
		(*m_uboDeferredDirectLighting)["LightCosHalfAngle"].Set(cosf(light.halfAngle));
		m_uboDeferredDirectLighting->GetBuffer()->Unmap();

		m_screenTriangle->Draw();
	}

	gl::Disable(gl::Cap::BLEND);
}

void Renderer::DrawScene()
{
	Model::BindVAO();

	for (const std::shared_ptr<Model>& model : m_scene->GetModels())
	{
		model->BindBuffers();
		for (const Model::Mesh& mesh : model->GetMeshes())
		{
			if (mesh.diffuseTexture)
				mesh.diffuseTexture->Bind(0);
			else
				gl::Texture2D::ResetBinding(0);
			GL_CALL(glDrawElements, GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(sizeof(std::uint32_t) * mesh.startIndex));
		}
	}
}

void Renderer::SetTrackLightCacheHashCollionCount(bool trackLightCacheHashCollisionCount)
{
	m_trackLightCacheHashCollisionCount = trackLightCacheHashCollisionCount;

	// Patch shader
	std::string defines = m_trackLightCacheHashCollisionCount ? "#define COUNT_LIGHTCACHE_HASH_COLLISIONS" : "";
	m_shaderFillLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/filllightcaches.frag", defines);
	m_shaderFillLightCaches->CreateProgram();
}
