#pragma once

#include <memory>
#include <ei/vector.hpp>
#include "camera/camera.hpp"

namespace gl
{
	class FramebufferObject;
	class ShaderObject;
	class ScreenAlignedTriangle;
	class Texture2D;
	class SamplerObject;
	class UniformBufferView;
}
class Scene;
class Voxelization;

class Renderer
{
public:
	Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution);
	~Renderer();

	void OnScreenResize(const ei::UVec2& newResolution);

	void SetScene(const std::shared_ptr<const Scene>& scene);

	void Draw(const Camera& camera);

private:
	std::shared_ptr<const Scene> m_scene;
	
	void UpdateConstantUBO(); 
	void UpdatePerFrameUBO(const Camera& camera);

	void DrawSceneToGBuffer();
	void DrawScene();

	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;
	std::unique_ptr<Voxelization> m_voxelization;


	std::unique_ptr<gl::ShaderObject> m_shaderDebugGBuffer;
	std::unique_ptr<gl::ShaderObject> m_shaderFillGBuffer_noskinning;
	

	std::unique_ptr<gl::UniformBufferView> m_uboConstant;
	std::unique_ptr<gl::UniformBufferView> m_uboPerFrame;

	std::unique_ptr<gl::Texture2D> m_GBuffer_diffuse;
	std::unique_ptr<gl::Texture2D> m_GBuffer_normal;
	std::unique_ptr<gl::Texture2D> m_GBuffer_depth;
	std::unique_ptr<gl::FramebufferObject> m_GBuffer;

	const gl::SamplerObject& m_samplerLinear;
};

