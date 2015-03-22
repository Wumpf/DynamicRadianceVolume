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
	m_samplerNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::REPEAT)))
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
	m_voxelization = std::make_unique<Voxelization>(64);

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

	m_shaderRequestLightCaches = std::make_unique<gl::ShaderObject>("request light caches");
	m_shaderRequestLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderRequestLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/requestlightcaches.frag");
	m_shaderRequestLightCaches->CreateProgram();

	m_shaderApplyLightCaches = std::make_unique<gl::ShaderObject>("apply light caches");
	m_shaderApplyLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderApplyLightCaches->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/applylightcaches.frag");
	m_shaderApplyLightCaches->CreateProgram();


	

	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), 
						m_shaderTonemap.get(), m_shaderRequestLightCaches.get(), m_shaderApplyLightCaches.get() };
	for (auto it : m_allShaders)
		ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(it);
}

void Renderer::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min - 0.1f);
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max + 0.1f);

	m_uboConstant->GetBuffer()->Map();
	(*m_uboConstant)["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	(*m_uboConstant)["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	(*m_uboConstant)["VoxelResolution"].Set(m_voxelization->GetVoxelTexture().GetWidth());
	(*m_uboConstant)["VoxelSizeInWorld"].Set((voxelVolumeWorldMax - voxelVolumeWorldMin) / m_voxelization->GetVoxelTexture().GetWidth());
	m_uboConstant->GetBuffer()->Unmap();
}

void Renderer::UpdatePerFrameUBO(const Camera& camera)
{
	auto view = camera.ComputeViewMatrix();
	auto projection = camera.ComputeProjectionMatrix();
	auto viewProjection = projection * view;

	m_uboPerFrame->GetBuffer()->Map();
	(*m_uboPerFrame)["Projection"].Set(projection);
	(*m_uboPerFrame)["ViewProjection"].Set(viewProjection);
	(*m_uboPerFrame)["InverseViewProjection"].Set(ei::invert(viewProjection));
	(*m_uboPerFrame)["CameraPosition"].Set(camera.GetPosition());
	(*m_uboPerFrame)["CameraDirection"].Set(camera.GetDirection());
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
	std::int32_t maxNumLightCaches = (newResolution.x / 2) * (newResolution.y / 2);
	m_voxelization->SetLightCacheSize(maxNumLightCaches);
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

	WriteCacheRequests();
	m_voxelization->VoxelizeAndCreateCaches(*m_scene);

	m_HDRBackbuffer->Bind(true);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	//DrawLights();
	//DrawGBufferDebug();

	// TODO
	m_shaderApplyLightCaches->BindSSBO(m_voxelization->GetLightCaches());
	m_shaderApplyLightCaches->Activate();
	m_screenTriangle->Draw();
	
	OutputHDRTextureToBackbuffer();

	//m_voxelization->DrawVoxelRepresentation();
}

void Renderer::WriteCacheRequests()
{
	gl::Disable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);
	gl::Disable(gl::Cap::CULL_FACE);
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	m_voxelization->GetVoxelTexture().ClearToZero();
	m_voxelization->GetVoxelTexture().BindImage(0, gl::Texture::ImageAccess::WRITE);
	m_shaderRequestLightCaches->Activate();
	m_screenTriangle->Draw();

	//GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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

void Renderer::SetTrackLightCacheCreationStats(bool trackLightCacheHashCollisionCount)
{
	m_voxelization->SetTrackLightCacheCreationStats(trackLightCacheHashCollisionCount);
}

bool Renderer::GetTrackLightCacheCreationStats() const
{
	return m_voxelization->GetTrackLightCacheCreationStats();
}

unsigned int Renderer::GetLightCacheHashCollisionCount() const
{
	return m_voxelization->GetLightCacheHashCollisionCount();
}

unsigned int Renderer::GetLightCacheActiveCount() const
{
	return m_voxelization->GetLightCacheActiveCount();
}