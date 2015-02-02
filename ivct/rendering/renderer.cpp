#include "renderer.hpp"

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
																				gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Border::REPEAT))),
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP)))
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_simpleShader = std::make_unique<gl::ShaderObject>("simple draw");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/simple.vert");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/simple.frag");
	m_simpleShader->CreateProgram();

	m_voxelizationShader = std::make_unique<gl::ShaderObject>("voxelization");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_voxelizationShader->CreateProgram();

	m_voxelDebugShader = std::make_unique<gl::ShaderObject>("voxel debug");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_voxelDebugShader->CreateProgram();

	m_constantUniformBuffer.Init(*m_simpleShader, "Constant");
	m_constantUniformBuffer.BindBuffer(0);

	m_perFrameUniformBuffer.Init(*m_simpleShader, "PerFrame");
	m_perFrameUniformBuffer.BindBuffer(1);

	// size for testing only. R8 sounds also like a bad idea since quite everything works in (at least) 32bytes on gpu
	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(512, 256, 256, gl::TextureFormat::R8, 1);


	// Register all shader for auto reload on change.
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_simpleShader.get());
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_voxelDebugShader.get());
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_voxelizationShader.get());


	SetScene(scene);
	OnScreenResize(resolution);
}

Renderer::~Renderer()
{
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_simpleShader.get());
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_voxelDebugShader.get());
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_voxelizationShader.get());
}

void Renderer::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_scene->GetBoundingBox().min);
	ei::Vec3 voxelVolumeWorldMax(m_scene->GetBoundingBox().max);
	ei::Vec3 voxelVolumeSizePix(static_cast<float>(m_voxelSceneTexture->GetWidth()), static_cast<float>(m_voxelSceneTexture->GetHeight()), static_cast<float>(m_voxelSceneTexture->GetDepth()));

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

void Renderer::DrawVoxelRepresentation()
{
	// Disable depthbuffering.
	GL_CALL(glDisable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	m_samplerLinearMipNearest.BindSampler(0);
	m_voxelSceneTexture->Bind(0);
	m_voxelDebugShader->Activate();

	m_screenTriangle->Draw();

	// Reenable depth buffer.
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
}

void Renderer::VoxelizeScene()
{
	// Disable depthbuffering & culling
	GL_CALL(glDisable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);
	GL_CALL(glDisable, GL_CULL_FACE);

	// Disable color write
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Viewport in size of voxel volume.
	// Using max on all dimensions may lead to simultaneous overwrites, but allows the geometry shader to flip triangles around much easier.
	auto maxDim = std::max(std::max(m_voxelSceneTexture->GetWidth(), m_voxelSceneTexture->GetHeight()), m_voxelSceneTexture->GetDepth());
	GL_CALL(glViewport, 0, 0, maxDim, maxDim);

	// Clear volume.
	m_voxelSceneTexture->ClearToZero();

	// Draw
	m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::WRITE);
	m_voxelizationShader->Activate();
	DrawScene();

	// Reset viewport
	GL_CALL(glViewport, 0, 0, 1024, 768); // TODO

	// Reenable depth buffer & culling
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
	GL_CALL(glEnable, GL_CULL_FACE);

	// Reenable color write
	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Renderer::Draw(const Camera& camera)
{
	UpdatePerFrameUBO(camera);

	//VoxelizeScene();
	//DrawVoxelRepresentation();

	m_simpleShader->Activate();
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