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
#include <glhelper/uniformbuffer.hpp>


Renderer::Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution) :
	m_samplerLinear(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																				gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Border::REPEAT)))
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_simpleShader = std::make_unique<gl::ShaderObject>("simple draw");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/simple.vert");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/simple.frag");
	m_simpleShader->CreateProgram();

	m_constantUniformBuffer.Init(*m_simpleShader, "Constant");
	m_constantUniformBuffer.BindBuffer(0);

	m_perFrameUniformBuffer.Init(*m_simpleShader, "PerFrame");
	m_perFrameUniformBuffer.BindBuffer(1);

	// Register all shader for auto reload on change.
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_simpleShader.get());


	m_voxelization = std::make_unique<Voxelization>(ei::UVec3(256, 256, 256));

	SetScene(scene);
	OnScreenResize(resolution);
}

Renderer::~Renderer()
{
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_simpleShader.get());
}

void Renderer::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min);
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max);
	ei::Vec3 voxelVolumeSizePix(static_cast<float>(m_voxelization->GetVoxelTexture().GetWidth()), 
								static_cast<float>(m_voxelization->GetVoxelTexture().GetHeight()), 
								static_cast<float>(m_voxelization->GetVoxelTexture().GetDepth()));
	

	m_constantUniformBuffer.GetBuffer()->Map();
	m_constantUniformBuffer["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	m_constantUniformBuffer["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	m_constantUniformBuffer["VoxelSizeInWorld"].Set((voxelVolumeWorldMax - voxelVolumeWorldMin) / voxelVolumeSizePix);
	m_constantUniformBuffer.GetBuffer()->Unmap();
}

void Renderer::UpdatePerFrameUBO(const Camera& camera)
{
	auto viewProjection = camera.ComputeViewProjection();

	m_perFrameUniformBuffer.GetBuffer()->Map();
	m_perFrameUniformBuffer["ViewProjection"].Set(viewProjection);
	m_perFrameUniformBuffer["InverseViewProjection"].Set(ei::invert(viewProjection));
	m_perFrameUniformBuffer["CameraPosition"].Set(camera.GetPosition());
	m_perFrameUniformBuffer.GetBuffer()->Unmap();
}

void Renderer::OnScreenResize(const ei::UVec2& newResolution)
{
	// Resize buffers etc.
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
	GL_CALL(glViewport, 0, 0, 1024, 768); // TODO
	m_voxelization->DrawVoxelRepresentation();

	//m_simpleShader->Activate();
	//DrawScene();
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