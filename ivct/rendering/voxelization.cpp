#include "voxelization.hpp"

#include "../scene/model.hpp"
#include "../scene/scene.hpp"

#include "../shaderfilewatcher.hpp"

#include <algorithm>

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/screenalignedtriangle.hpp>


Voxelization::Voxelization(const ei::UVec3& resolution) :
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP)))
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_voxelizationShader = std::make_unique<gl::ShaderObject>("voxelization");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_voxelizationShader->CreateProgram();

	m_voxelDebugShader = std::make_unique<gl::ShaderObject>("voxel debug");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_voxelDebugShader->CreateProgram();

	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(resolution.x, resolution.y, resolution.z, gl::TextureFormat::R8, 1);

	// Register all shader for auto reload on change.
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_voxelDebugShader.get());
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_voxelizationShader.get());

}

Voxelization::~Voxelization()
{
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_voxelDebugShader.get());
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_voxelizationShader.get());
}


void Voxelization::DrawVoxelRepresentation()
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

void Voxelization::VoxelizeScene(const Scene& scene)
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
	Model::BindVAO();
	for (const std::shared_ptr<Model>& model : scene.GetModels())
	{
		model->BindBuffers();
		GL_CALL(glDrawElements, GL_TRIANGLES, model->GetNumTriangles() * 3, GL_UNSIGNED_INT, nullptr);
	}

	// Reenable depth buffer & culling
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
	GL_CALL(glEnable, GL_CULL_FACE);

	// Reenable color write
	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}