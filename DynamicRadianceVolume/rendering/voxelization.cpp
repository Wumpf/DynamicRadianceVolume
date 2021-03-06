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
#include "../time/Time.h"


Voxelization::Voxelization(unsigned int resolution) :
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP))),
	m_adaptionRate(10.0f)
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

	m_shaderVoxelMipMap = new gl::ShaderObject("voxel mipmap");
	m_shaderVoxelMipMap->AddShaderFromFile(gl::ShaderObject::ShaderType::COMPUTE, "shader/voxelmipmap.comp");
	m_shaderVoxelMipMap->CreateProgram();

	SetResolution(resolution);
}

Voxelization::~Voxelization()
{
}


void Voxelization::SetResolution(unsigned int resolution)
{
	m_voxelSceneTextureTarget = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8, 1);
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
	double timeSinceLastBlend = ezTime::Now().GetSeconds() - m_lastUpdateTime;
	double adaptionThisFrame = timeSinceLastBlend * m_adaptionRate * 255.0;
	
	// Actual adaption is floored.
	double adaptionThisFrameFloor = floor(adaptionThisFrame);
	// But we do not want to loose this and add it to the next frame! This is done by virtually doing this update earlier.
	m_lastUpdateTime = ezTime::Now().GetSeconds() - (adaptionThisFrame - adaptionThisFrameFloor) / m_adaptionRate / 255.0;

	if (adaptionThisFrameFloor > 0.0f)
	{
		{
			PROFILE_GPU_SCOPED(VoxelizeScene);

			m_voxelSceneTextureTarget->ClearToZero();

			// Disable depthbuffering & culling
			gl::Disable(gl::Cap::DEPTH_TEST);
			gl::SetDepthWrite(false);
			gl::Disable(gl::Cap::CULL_FACE);
			GL_CALL(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // The single one case where z -1 to 1 is actual practical. Makes the shader easier

			// Disable color write
			GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

			// Viewport in size of voxel volume.
			gl::FramebufferObject::BindBackBuffer(); // a lower res target might be bound otherwise
			GL_CALL(glViewport, 0, 0, m_voxelSceneTextureTarget->GetWidth(), m_voxelSceneTextureTarget->GetWidth());

			// Draw
			GL_CALL(glMemoryBarrier, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // Ensure Cache demands are written.
			m_voxelSceneTextureTarget->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);
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

		{
			PROFILE_GPU_SCOPED(VoxelBlendMipMap);

			// Blend
			m_voxelSceneTextureTarget->Bind(0);
			m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);
			m_shaderVoxelBlend->Activate();
			GL_CALL(glUniform1f, 0, static_cast<float>(adaptionThisFrameFloor / 255.0));
			GL_CALL(glDispatchCompute, m_voxelSceneTexture->GetWidth() / 8, m_voxelSceneTexture->GetHeight() / 8, m_voxelSceneTexture->GetDepth() / 8);


			// Mipmap
			m_shaderVoxelMipMap->Activate();
			m_samplerLinearMipNearest.BindSampler(0);
			m_voxelSceneTexture->Bind(0);
			unsigned int targetResolution = GetResolution();
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			for (GLsizei readLevel = 0; readLevel < m_voxelSceneTexture->GetNumMipLevels()-1; ++readLevel)
			{
				targetResolution /= 2;

				GL_CALL(glUniform1f, 0, 1.0f / static_cast<float>(targetResolution));
				GL_CALL(glTextureParameteri, m_voxelSceneTexture->GetInternHandle(), GL_TEXTURE_BASE_LEVEL, readLevel);
				m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::WRITE, readLevel+1);

				unsigned int threadBlockCount = ei::max<unsigned int>(1, targetResolution / 8);
				GL_CALL(glDispatchCompute, threadBlockCount, threadBlockCount, threadBlockCount);
			}
			GL_CALL(glTextureParameteri, m_voxelSceneTexture->GetInternHandle(), GL_TEXTURE_BASE_LEVEL, 0);

		}
	}
}
