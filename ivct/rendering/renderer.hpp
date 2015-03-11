#pragma once

#include <memory>
#include <vector>
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
	class ShaderStorageBufferView;
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

	void SetTrackLightCacheHashCollionCount(bool trackLightCacheHashCollisionCount);
	bool GetTrackLightCacheHashCollionCount() const { return m_trackLightCacheHashCollisionCount; }
	unsigned int GetLightCacheHashCollisionCount() const { return m_lastLightCacheHashCollisionCount; }

private:
	std::shared_ptr<const Scene> m_scene;

	void LoadShader();

	void UpdateConstantUBO(); 
	void UpdatePerFrameUBO(const Camera& camera);

	/// Fills GBuffer.
	void DrawSceneToGBuffer();
	/// Draws GBuffer directly to the (hardware) backbuffer.
	void DrawGBufferDebug();
	/// Performs direct lighting for all lights.
	void DrawLights();
	/// Fills light caches with screen space information.
	void FillLightCaches();

	void OutputHDRTextureToBackbuffer();

	/// Draws scene, mesh by mesh.
	///
	/// Does set VAO, VBO and index buffers but nothing else. No culling!
	void DrawScene();

	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;
	std::unique_ptr<Voxelization> m_voxelization;


	std::unique_ptr<gl::ShaderObject> m_shaderDebugGBuffer;
	std::unique_ptr<gl::ShaderObject> m_shaderFillGBuffer_noskinning;

	std::unique_ptr<gl::ShaderObject> m_shaderDeferredDirectLighting_Spot;
	std::unique_ptr<gl::UniformBufferView> m_uboDeferredDirectLighting;

	std::unique_ptr<gl::UniformBufferView> m_uboConstant;
	std::unique_ptr<gl::UniformBufferView> m_uboPerFrame;



	std::unique_ptr<gl::ShaderObject> m_shaderFillLightCaches;

	bool m_trackLightCacheHashCollisionCount;
	unsigned int m_lastLightCacheHashCollisionCount;
	std::unique_ptr<gl::ShaderStorageBufferView> m_lightCacheHashCollisionCounter; ///< Atomic hash collision counter for debugging purposes
	
	ei::Vec3 m_cacheWorldSize;
	std::unique_ptr<gl::ShaderStorageBufferView> m_lightCaches;


	std::unique_ptr<gl::Texture2D> m_texturePerPixelCacheEntries;
	std::unique_ptr<gl::FramebufferObject> m_fboPerPixelCacheEntries;

	std::unique_ptr<gl::Texture2D> m_GBuffer_diffuse;
	std::unique_ptr<gl::Texture2D> m_GBuffer_normal;
	std::unique_ptr<gl::Texture2D> m_GBuffer_depth;
	std::unique_ptr<gl::FramebufferObject> m_GBuffer;

	std::unique_ptr<gl::Texture2D> m_HDRBackbufferTexture;
	std::unique_ptr<gl::FramebufferObject> m_HDRBackbuffer;
	std::unique_ptr<gl::ShaderObject> m_shaderTonemap;

	const gl::SamplerObject& m_samplerLinear;
	const gl::SamplerObject& m_samplerNearest;


	/// List of all shaders for convenience purposes.
	std::vector<gl::ShaderObject*> m_allShaders;
};

