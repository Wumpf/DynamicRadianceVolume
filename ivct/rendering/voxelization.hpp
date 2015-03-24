#pragma once

#include <memory>
#include <ei/vector.hpp>
#include "camera/camera.hpp"

namespace gl
{
	class FramebufferObject;
	class ShaderObject;
	class ScreenAlignedTriangle;
	class Texture3D;
	class ShaderStorageBufferView;
	class SamplerObject;
}
class Renderer;

/// Voxelization + LightCache generation.
/// Not exactly self-contained! Submodule for renderer!
class Voxelization
{
public:
	Voxelization(unsigned int resolution);
	~Voxelization();

	/// Sets maximal number of light caches.
	///
	/// Reallocates internal buffer accordingly.
	void SetLightCacheSize(unsigned int maxNumLightCaches);

	/// Voxel debug output.
	void DrawVoxelRepresentation();


	/// Voxelizes given scene into current voxel grid.
	/// 
	/// Caches are created on previously marked voxels (no clear happens here!)
	/// Grid world size settings are currently handled via the global "Constant" ubo.
	/// Changed renderstates: Viewport, VAO, depth write/read off, culling off.
	void VoxelizeAndCreateCaches(Renderer& renderer);


	void SetTrackLightCacheCreationStats(bool trackLightCacheCreationStats);
	bool GetTrackLightCacheCreationStats() const			{ return m_trackLightCacheCreationStats; }
	unsigned int GetLightCacheHashCollisionCount() const	{ return m_lastLightCacheHashCollisionCount; }
	unsigned int GetLightCacheActiveCount() const			{ return m_lastLightCacheActiveCount; }

	gl::ShaderStorageBufferView& GetLightCaches() { return *m_lightCaches; }

	const gl::Texture3D& GetVoxelTexture() const { return *m_voxelSceneTexture; }
	gl::Texture3D& GetVoxelTexture()			 { return *m_voxelSceneTexture; }

private:
	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<gl::ShaderObject> m_shaderVoxelize;
	std::unique_ptr<gl::ShaderObject> m_shaderVoxelDebug;

	std::unique_ptr<gl::Texture3D> m_voxelSceneTexture;


	std::unique_ptr<gl::ShaderStorageBufferView> m_lightCaches;

	bool m_trackLightCacheCreationStats;
	unsigned int m_lastLightCacheHashCollisionCount;
	unsigned int m_lastLightCacheActiveCount;
	std::unique_ptr<gl::ShaderStorageBufferView> m_lightCacheHashCollisionCounter; ///< Atomic hash collision counter for debugging purposes


	const gl::SamplerObject& m_samplerLinearMipNearest;
};

