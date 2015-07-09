#pragma once

#include <memory>
#include <ei/vector.hpp>
#include "camera/camera.hpp"
#include "../shaderreload/autoreloadshaderptr.hpp"

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

	void SetResolution(unsigned int resolution);
	unsigned int GetResolution() const;

	void SetAdaptionRate(float adaptionRate)	{ m_adaptionRate = adaptionRate; }
	float GetAdaptionRate() const				{ return m_adaptionRate; }

	/// Voxel debug output.
	void DrawVoxelRepresentation();

	/// Voxelizes given scene into current voxel grid.
	void VoxelizeScene(Renderer& renderer);

	const gl::Texture3D& GetVoxelTexture() const { return *m_voxelSceneTexture; }
	gl::Texture3D& GetVoxelTexture()			 { return *m_voxelSceneTexture; }

private:
	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	float m_adaptionRate;
	double m_lastUpdateTime;

	AutoReloadShaderPtr m_shaderVoxelize;
	AutoReloadShaderPtr m_shaderVoxelDebug;
	AutoReloadShaderPtr m_shaderVoxelBlend;

	std::unique_ptr<gl::Texture3D> m_voxelSceneTextureTarget;
	std::unique_ptr<gl::Texture3D> m_voxelSceneTexture;

	//std::vector<std::shared_ptr<gl::FramebufferObject>> m_voxelSceneSliceFBOs;

	const gl::SamplerObject& m_samplerLinearMipNearest;
};

