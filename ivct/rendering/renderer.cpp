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

	m_shaderDebugGBuffer = std::make_unique<gl::ShaderObject>("gbuffer debug");
	m_shaderDebugGBuffer->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderDebugGBuffer->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/debuggbuffer.frag");
	m_shaderDebugGBuffer->CreateProgram();

	m_shaderFillGBuffer_noskinning = std::make_unique<gl::ShaderObject>("fill gbuffer noskinning");
	m_shaderFillGBuffer_noskinning->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/defaultmodel_noskinning.vert");
	m_shaderFillGBuffer_noskinning->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/fillgbuffer.frag");
	m_shaderFillGBuffer_noskinning->CreateProgram();

	std::initializer_list<const gl::ShaderObject*> shaderList = { m_shaderDebugGBuffer.get(), m_shaderFillGBuffer_noskinning.get() };

	m_uboConstant = std::make_unique<gl::UniformBufferView>(shaderList, "Constant");
	m_uboConstant->BindBuffer(0);

	m_uboPerFrame = std::make_unique<gl::UniformBufferView>(shaderList, "PerFrame");
	m_uboPerFrame->BindBuffer(1);

	// Register all shader for auto reload on change.
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shaderDebugGBuffer.get());
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shaderFillGBuffer_noskinning.get());


	m_voxelization = std::make_unique<Voxelization>(ei::UVec3(256, 256, 256));

	SetScene(scene);
	OnScreenResize(resolution);
}

Renderer::~Renderer()
{
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_shaderDebugGBuffer.get());
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_shaderFillGBuffer_noskinning.get());
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
	auto viewProjection = camera.ComputeViewProjection();

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

	glDisable(GL_DEPTH_TEST);

	m_shaderDebugGBuffer->Activate();
	gl::FramebufferObject::BindBackBuffer();
	m_GBuffer_diffuse->Bind(0);
	m_GBuffer_normal->Bind(1);
	m_GBuffer_depth->Bind(2);
	m_screenTriangle->Draw();

	glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawSceneToGBuffer()
{
	m_shaderFillGBuffer_noskinning->Activate();
	m_GBuffer->Bind(false);
	GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene();
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