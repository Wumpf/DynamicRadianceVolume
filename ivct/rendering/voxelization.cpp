#include "voxelization.hpp"

#include "../scene/model.hpp"
#include "../scene/scene.hpp"

#include "../shaderfilewatcher.hpp"

#include <algorithm>

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/buffer.hpp>
#include <glhelper/shaderstoragebufferview.hpp>
#include <glhelper/screenalignedtriangle.hpp>
#include <glhelper/statemanagement.hpp>
#include <glhelper/utils/flagoperators.hpp>



Voxelization::Voxelization(unsigned int resolution) :
	m_trackLightCacheCreationStats(false),
	m_lastLightCacheHashCollisionCount(0),
	m_lastLightCacheActiveCount(0),
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR,
																							gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP)))
{
	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_shaderVoxelize = std::make_unique<gl::ShaderObject>("voxelization");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_shaderVoxelize->CreateProgram();

	m_shaderVoxelDebug = std::make_unique<gl::ShaderObject>("voxel debug");
	m_shaderVoxelDebug->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_shaderVoxelDebug->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_shaderVoxelDebug->CreateProgram();

	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(resolution, resolution, resolution, gl::TextureFormat::R8UI, 1);

	// misc buffer
	m_lightCacheHashCollisionCounter = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(sizeof(std::int32_t) * 2, 
																	gl::Buffer::Usage::MAP_READ | gl::Buffer::Usage::MAP_WRITE), "LightCacheStats");


	// Register all shader for auto reload on change.
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shaderVoxelDebug.get());
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shaderVoxelize.get());
}

Voxelization::~Voxelization()
{
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_shaderVoxelDebug.get());
	ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_shaderVoxelize.get());
}

void Voxelization::SetLightCacheSize(unsigned int maxNumLightCaches)
{
	static const std::int32_t lightCacheEntrySize = sizeof(float) * 4 * 1;
	m_lightCaches = std::make_unique<gl::ShaderStorageBufferView>(std::make_shared<gl::Buffer>(lightCacheEntrySize * maxNumLightCaches, gl::Buffer::Usage::IMMUTABLE), "LightCaches");
}

void Voxelization::DrawVoxelRepresentation()
{
	// Disable depthbuffering.
	gl::Disable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	m_samplerLinearMipNearest.BindSampler(0);
	m_voxelSceneTexture->Bind(0);
	m_shaderVoxelDebug->Activate();

	m_screenTriangle->Draw();

	// Reenable depth buffer.
	gl::Enable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
}

void Voxelization::VoxelizeAndCreateCaches(const Scene& scene)
{
	// Disable depthbuffering & culling
	gl::Disable(gl::Cap::DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);
	gl::Disable(gl::Cap::CULL_FACE);
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // The single one case where z -1 to 1 is actual practical. Makes the shader easier

	// Disable color write
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Viewport in size of voxel volume.
	GL_CALL(glViewport, 0, 0, m_voxelSceneTexture->GetWidth(), m_voxelSceneTexture->GetWidth());


	// Light cache setup
	if (m_lightCaches)
	{
		// Light cache hash collision counter
		if (m_trackLightCacheCreationStats)
		{
			std::uint32_t* hashCollisionCount = static_cast<std::uint32_t*>(m_lightCacheHashCollisionCounter->GetBuffer()->Map());
			m_lastLightCacheHashCollisionCount = hashCollisionCount[0];
			m_lastLightCacheActiveCount = hashCollisionCount[1];
			hashCollisionCount[0] = 0;
			hashCollisionCount[1] = 0;
			m_lightCacheHashCollisionCounter->GetBuffer()->Unmap();

			m_shaderVoxelize->BindSSBO(*m_lightCacheHashCollisionCounter);
		}

		m_lightCaches->GetBuffer()->ClearToZero();
		m_shaderVoxelize->BindSSBO(*m_lightCaches);
	}

	// Draw
	GL_CALL(glMemoryBarrier, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // Ensure Cache demands are written.
	m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::READ_WRITE);
	m_shaderVoxelize->Activate();
	Model::BindVAO();
	for (const std::shared_ptr<Model>& model : scene.GetModels())
	{
		model->BindBuffers();
		GL_CALL(glDrawElements, GL_TRIANGLES, model->GetNumTriangles() * 3, GL_UNSIGNED_INT, nullptr);
	}

	// Reset to default (convention)
	GL_CALL(glClipControl, GL_LOWER_LEFT, GL_ZERO_TO_ONE); 

	// Reenable color write
	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Voxelization::SetTrackLightCacheCreationStats(bool trackLightCacheCreationStats)
{
	m_trackLightCacheCreationStats = trackLightCacheCreationStats;

	// Patch shader
	std::string defines = m_trackLightCacheCreationStats ? "#define LIGHTCACHE_CREATION_STATS" : "";
	m_shaderVoxelize->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag", defines);
	m_shaderVoxelize->CreateProgram();
}
