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
#include <glhelper/framebufferobject.hpp>


Renderer::Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution) :
m_samplerLinear(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Border::REPEAT)))
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

	// General GL settings
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDisable, GL_DITHER);
	//GL_CALL(glEnable, GL_CULL_FACE);
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
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

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

	


	// Register all shader for auto reload on change.
	m_allShaders = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get(), m_shaderDeferredDirectLighting_Spot.get(), m_shaderTonemap.get() };
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

	/*m_voxelization->VoxelizeScene(*m_scene);
	GL_CALL(glViewport, 0, 0, 1024, 768); // TODO
	m_voxelization->DrawVoxelRepresentation();*/

	DrawSceneToGBuffer();
	
	m_HDRBackbuffer->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT);
	
	DrawLights();


	// Output HDR texture to backbuffer.
	gl::FramebufferObject::BindBackBuffer();
	glEnable(GL_FRAMEBUFFER_SRGB);
	m_shaderTonemap->Activate();
	m_HDRBackbufferTexture->Bind(0);
	m_screenTriangle->Draw();
	glDisable(GL_FRAMEBUFFER_SRGB);
}

void Renderer::DrawSceneToGBuffer()
{
	glEnable(GL_DEPTH_TEST);

	m_shaderFillGBuffer_noskinning->Activate();
	m_GBuffer->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene();
}

void Renderer::DrawGBufferDebug()
{
	glDisable(GL_DEPTH_TEST);

	m_shaderDebugGBuffer->Activate();
	gl::FramebufferObject::BindBackBuffer();
	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	m_screenTriangle->Draw();
}

void Renderer::DrawLights()
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
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
		(*m_uboDeferredDirectLighting)["LightDirection"].Set(light.direction);
		(*m_uboDeferredDirectLighting)["LightCosHalfAngle"].Set(cosf(light.halfAngle));
		m_uboDeferredDirectLighting->GetBuffer()->Unmap();

		m_screenTriangle->Draw();
	}

	glDisable(GL_BLEND);
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
			GL_CALL(glDrawElements, GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(sizeof(std::uint32_t) * mesh.startIndex));
		}
	}
}