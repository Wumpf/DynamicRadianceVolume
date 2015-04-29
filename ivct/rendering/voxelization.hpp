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

	/// Voxel debug output.
	void DrawVoxelRepresentation();

	/// Voxelizes given scene into current voxel grid.
	/// 
	/// Caches are created on previously marked voxels (no clear happens here!)
	/// Grid world size settings are currently handled via the global "Constant" ubo.
	/// Changed renderstates: Viewport, VAO, depth write/read off, culling off.
	void VoxelizeScene(Renderer& renderer);

	void GenMipMap();

	const gl::Texture3D& GetVoxelTexture() const { return *m_voxelSceneTexture; }
	gl::Texture3D& GetVoxelTexture()			 { return *m_voxelSceneTexture; }

private:
	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<gl::ShaderObject> m_shaderVoxelize;
	std::unique_ptr<gl::ShaderObject> m_shaderVoxelDebug;

	std::unique_ptr<gl::Texture3D> m_voxelSceneTexture;

	const gl::SamplerObject& m_samplerLinearMipNearest;
};

