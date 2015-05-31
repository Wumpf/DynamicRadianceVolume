#include "voxelization.hpp"

#include "../scene/model.hpp"
#include "../scene/scene.hpp"
#include "renderer.hpp"

#include <algorithm>

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/buffer.hpp>
#include <glhelper/screenalignedtriangle.hpp>
#include <glhelper/statemanagement.hpp>
#include <glhelper/utils/flagoperators.hpp>
#include <glhelper/framebufferobject.hpp>

#include "../frameprofiler.hpp"


Voxelization::Voxelization(unsigned int resolution) :
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP)))
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_shaderVoxelize = new gl::ShaderObject("voxelization");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_shaderVoxelize->CreateProgram();

	m_shaderVoxelDebug = new gl::ShaderObject("voxel debug");
	m_shaderVoxelDebug->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderVoxelDebug->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_shaderVoxelDebug->CreateProgram();

	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8, 0);
}

Voxelization::~Voxelization()
{
}

void Voxelization::DrawVoxelRepresentation()
{
	// Disable depthbuffering.
	gl::Disable(gl::Cap::DEPTH_TEST);
	gl::SetDepthWrite(false);
	gl::FramebufferObject::BindBackBuffer();

	m_samplerLinearMipNearest.BindSampler(0);
	m_voxelSceneTexture->Bind(0);
	m_shaderVoxelDebug->Activate();

	m_screenTriangle->Draw();

	// Reenable depth buffer.
	gl::Enable(gl::Cap::DEPTH_TEST);
	gl::SetDepthWrite(true);
}

void Voxelization::VoxelizeScene(Renderer& renderer)
{
	PROFILE_GPU_SCOPED(VoxelizeScene);

	m_voxelSceneTexture->ClearToZero();

	// Disable depthbuffering & culling
	gl::Disable(gl::Cap::DEPTH_TEST);
	gl::SetDepthWrite(false);
	gl::Disable(gl::Cap::CULL_FACE);
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // The single one case where z -1 to 1 is actual practical. Makes the shader easier

	// Disable color write
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Viewport in size of voxel volume.
	gl::FramebufferObject::BindBackBuffer(); // a lower res target might be bound otherwise
	GL_CALL(glViewport, 0, 0, m_voxelSceneTexture->GetWidth(), m_voxelSceneTexture->GetWidth());

	// Draw
	GL_CALL(glMemoryBarrier, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // Ensure Cache demands are written.
	m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);
	m_shaderVoxelize->Activate();
	Model::BindVAO();

	for (unsigned int entityIndex = 0; entityIndex < renderer.GetScene()->GetEntities().size(); ++entityIndex)
	{
		const SceneEntity& entity = renderer.GetScene()->GetEntities()[entityIndex];
		if (!entity.GetModel())
			continue;

		renderer.BindObjectUBO(entityIndex);
		entity.GetModel()->BindBuffers();
		GL_CALL(glDrawElements, GL_TRIANGLES, entity.GetModel()->GetNumTriangles() * 3, GL_UNSIGNED_INT, nullptr);
	}

	// Reset to default (convention)
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_ZERO_TO_ONE); 

	// Reenable color write
	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Voxelization::GenMipMap()
{
	PROFILE_GPU_SCOPED(VoxelMipMap);
	m_voxelSceneTexture->GenMipMaps();
}
