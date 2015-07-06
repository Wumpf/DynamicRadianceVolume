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
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP))),
	m_refreshInterval(0.25f)
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

	m_shaderVoxelBlend = new gl::ShaderObject("voxel blend");
	m_shaderVoxelBlend->AddShaderFromFile(gl::ShaderObject::ShaderType::COMPUTE, "shader/voxelblend.comp");
	m_shaderVoxelBlend->CreateProgram();

	SetResolution(resolution);
}

Voxelization::~Voxelization()
{
}


void Voxelization::SetResolution(unsigned int resolution)
{
	m_voxelSceneTextureNew = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8, 1);
	m_voxelSceneTextureOld = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8, 1);
	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8, 0);

/*	m_voxelSceneSliceFBOs.clear();
	for (int i = 0; i < resolution; ++i)
	{
		m_voxelSceneSliceFBOs.push_back(std::make_shared<gl::FramebufferObject>(gl::FramebufferObject::Attachment(m_voxelSceneTexture.get(), 0, i)));
	}*/
}
unsigned int Voxelization::GetResolution() const
{
	return m_voxelSceneTexture->GetWidth();
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

	// New voxelization
	if (m_refreshStopwatch.GetRunningTotal().GetSeconds() >= m_refreshInterval)
	{
		m_refreshStopwatch.StopAndReset();
		m_refreshStopwatch.Resume();
		std::swap(m_voxelSceneTextureOld, m_voxelSceneTextureNew);

		m_voxelSceneTextureNew->ClearToZero();

		// Disable depthbuffering & culling
		gl::Disable(gl::Cap::DEPTH_TEST);
		gl::SetDepthWrite(false);
		gl::Disable(gl::Cap::CULL_FACE);
		GL_CALL(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // The single one case where z -1 to 1 is actual practical. Makes the shader easier

		// Disable color write
		GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		// Viewport in size of voxel volume.
		gl::FramebufferObject::BindBackBuffer(); // a lower res target might be bound otherwise
		GL_CALL(glViewport, 0, 0, m_voxelSceneTextureNew->GetWidth(), m_voxelSceneTextureNew->GetWidth());

		// Draw
		GL_CALL(glMemoryBarrier, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // Ensure Cache demands are written.
		m_voxelSceneTextureNew->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);
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
}

void Voxelization::BlendAndMipMap()
{
	PROFILE_GPU_SCOPED(VoxelBlendMipMap);

	m_voxelSceneTextureOld->Bind(0);
	m_voxelSceneTextureNew->Bind(1);
	m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::WRITE);
	m_shaderVoxelBlend->Activate();
	float blend = static_cast<float>(m_refreshStopwatch.GetRunningTotal().GetSeconds() / m_refreshInterval);
	GL_CALL(glUniform1f, 0, blend);
	GL_CALL(glDispatchCompute, m_voxelSceneTexture->GetWidth() / 8, m_voxelSceneTexture->GetHeight() / 8, m_voxelSceneTexture->GetDepth() / 8);


	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	m_voxelSceneTexture->GenMipMaps();
}
