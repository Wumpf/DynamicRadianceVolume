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
	class SamplerObject;
}
class Scene;

class Voxelization
{
public:
	Voxelization(const ei::UVec3& resolution);
	~Voxelization();

	void DrawVoxelRepresentation();

	/// Voxelizes given scene into current grid
	/// 
	/// Grid world size settings are currently handled via the global "Constant" ubo
	/// Attention: This function changes the viewport!
	void VoxelizeScene(const Scene& scene);

	const gl::Texture3D& GetVoxelTexture() const { return *m_voxelSceneTexture; }

private:
	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<gl::ShaderObject> m_voxelizationShader;
	std::unique_ptr<gl::ShaderObject> m_voxelDebugShader;

	std::unique_ptr<gl::Texture3D> m_voxelSceneTexture;

	const gl::SamplerObject& m_samplerLinearMipNearest;
};

