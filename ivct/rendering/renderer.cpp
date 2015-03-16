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

	m_shaderApplyLightCaches = std::make_unique<gl::ShaderObject>("apply light caches");
	m_shaderApplyLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderApplyLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/applylightcaches.frag");
	m_shaderApplyLightCaches->CreateProgram();

	m_shaderVoxelize = std::make_unique<gl::ShaderObject>("voxelization + cache creation");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_shaderVoxelize->CreateProgram();


	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), 
					 m_shaderTonemap.get(), m_shaderApplyLightCaches.get(), m_shaderVoxelize.get() };
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(it);
}

void Renderer::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min - ei::Vec3(1.0f));
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max + ei::Vec3(1.0f));
	m_voxelVolumeSize = ei::IVec3(64, 64, 64);
	ei::Vec3 voxelVolumeSizePix((voxelVolumeWorldMax - voxelVolumeWorldMin) / m_voxelVolumeSize);
	

	m_uboConstant->GetBuffer()->Map();
	(*m_uboConstant)["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	(*m_uboConstant)["VoxelCountX"].Set(m_voxelVolumeSize.x);
	(*m_uboConstant)["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	(*m_uboConstant)["VoxelCountXZ"].Set(m_voxelVolumeSize.x * m_voxelVolumeSize.z);
	(*m_uboConstant)["VoxelSizeInWorld"].Set(voxelVolumeSizePix);
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

	DrawSceneToGBuffer();


	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	gl::Disable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	VoxelizeAndCreateCaches();

	m_HDRBackbuffer->Bind(true);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	//DrawLights();
	//DrawGBufferDebug();

	// TODO
	m_shaderApplyLightCaches->Activate();
	m_screenTriangle->Draw();

	OutputHDRTextureToBackbuffer();
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

void Renderer::VoxelizeAndCreateCaches()
{
	if (m_trackLightCacheHashCollisionCount)
	{
		std::uint32_t* hashCollisionCount = static_cast<std::uint32_t*>(m_lightCacheHashCollisionCounter->GetBuffer()->Map());
		m_lastLightCacheHashCollisionCount = *hashCollisionCount;
		*hashCollisionCount = 0;
		m_lightCacheHashCollisionCounter->GetBuffer()->Unmap();

		m_shaderVoxelize->BindSSBO(*m_lightCacheHashCollisionCounter);
	}

	m_lightCaches->GetBuffer()->ClearToZero();
	m_shaderVoxelize->BindSSBO(*m_lightCaches);


	// Disable color write
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	// The single one case where z -1 to 1 is actual practical. Makes the shader easier
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);


	// Viewport in size of voxel volume.
	// Using max on all dimensions may lead to simultaneous overwrites, but allows the geometry shader to flip triangles around much easier.
	auto maxDim = ei::max(m_voxelVolumeSize);
	GL_CALL(glViewport, 0, 0, maxDim, maxDim);


	m_shaderVoxelize->Activate();
	DrawScene(false);

	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_ZERO_TO_ONE);
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

void Renderer::DrawScene(bool setTextures)
{
	Model::BindVAO();

	for (const std::shared_ptr<Model>& model : m_scene->GetModels())
	{
		model->BindBuffers();
		for (const Model::Mesh& mesh : model->GetMeshes())
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

void Renderer::SetTrackLightCacheHashCollionCount(bool trackLightCacheHashCollisionCount)
{
	m_trackLightCacheHashCollisionCount = trackLightCacheHashCollisionCount;

	// Patch shader
	std::string defines = m_trackLightCacheHashCollisionCount ? "#define COUNT_LIGHTCACHE_HASH_COLLISIONS" : "";
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag", defines);
	m_shaderVoxelize->CreateProgram();
}
