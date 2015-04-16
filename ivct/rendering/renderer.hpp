#pragma once

#include <memory>
#include <vector>
#include <ei/vector.hpp>
#include "camera/camera.hpp"
#include <glhelper/shaderdatametainfo.hpp>

namespace gl
{
	class FramebufferObject;
	class ShaderObject;
	class ScreenAlignedTriangle;
	class Texture2D;
	class Texture3D;
	class SamplerObject;
	class PersistentRingBuffer;
	class Buffer;
}
class Scene;
class SceneEntity;
class Voxelization;

typedef std::unique_ptr<gl::Texture2D> Texture2DPtr;
typedef std::unique_ptr<gl::ShaderObject> ShaderPtr;
typedef std::unique_ptr<gl::Buffer> BufferPtr;

class Renderer
{
public:
	Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution);
	~Renderer();

	void OnScreenResize(const ei::UVec2& newResolution);

	void SetScene(const std::shared_ptr<const Scene>& scene);
	const std::shared_ptr<const Scene>& GetScene() const { return m_scene; }

	void Draw(const Camera& camera);

	void SetReadLightCacheCount(bool trackLightCacheCreationStats);
	bool GetReadLightCacheCount() const;
	unsigned int GetLightCacheActiveCount() const;

	void BindObjectUBO(unsigned int objectIndex);

private:
	std::shared_ptr<const Scene> m_scene;

	void LoadShader();

	void UpdateConstantUBO(); 
	void UpdatePerFrameUBO(const Camera& camera);

	void UpdatePerObjectUBORingBuffer();
	void PrepareLights();

	/// Fills GBuffer.
	void DrawSceneToGBuffer();
	void DrawShadowMaps();

	/// Draws GBuffer directly to the (hardware) backbuffer.
	void DrawGBufferDebug();

	/// Performs direct lighting for all lights.
	void DrawLights();
	
	void OutputHDRTextureToBackbuffer();

	void GatherLightCaches();
	void ApplyLightCaches();

	/// Applies direct light to caches (mainly for debug purposes)
	void DirectCacheLighting();

	/// Draws scene, mesh by mesh.
	///
	/// Does set VAO, VBO and index buffers but nothing else. No culling!
	void DrawScene(bool setTextures);


	// ------------------------------------------------------------
	// Indirect lighting

	ShaderPtr m_shaderCacheGather;
	ShaderPtr m_shaderCacheApply;

	//unsigned int m_lightCacheHashMapSize;
	//std::unique_ptr<gl::ShaderStorageBufferView> m_lightCacheHashMap;

	std::unique_ptr<gl::Texture3D> m_lightCacheAddressVolume;

	unsigned int m_maxNumLightCaches;
	bool m_readLightCacheCount;
	unsigned int m_lastNumLightCaches;
	BufferPtr m_lightCacheBuffer;
	BufferPtr m_lightCacheCounter;

	ShaderPtr m_shaderLightCachesDirect;


	// ------------------------------------------------------------
	// Direct lighting / RSM

	ShaderPtr m_shaderDebugGBuffer;
	ShaderPtr m_shaderFillGBuffer_noskinning;

	ShaderPtr m_shaderDeferredDirectLighting_Spot;

	gl::UniformBufferMetaInfo m_uboInfoSpotLight;
	BufferPtr m_uboSpotLight;


	Texture2DPtr m_GBuffer_diffuse;
	Texture2DPtr m_GBuffer_normal;
	Texture2DPtr m_GBuffer_depth;
	std::unique_ptr<gl::FramebufferObject> m_GBuffer;

	struct ShadowMap
	{
		/// Initializes all textures and the fbo with the given resolution (square)
		/// Will recreate if already initialized.
		void Init(unsigned int resolution);

		Texture2DPtr irradiance;
		Texture2DPtr normal;
		Texture2DPtr depth;
		std::unique_ptr<gl::FramebufferObject> fbo;
	};
	std::vector<ShadowMap> m_shadowMaps; ///< A shadowmap for every light.

	Texture2DPtr m_HDRBackbufferTexture;
	std::unique_ptr<gl::FramebufferObject> m_HDRBackbuffer;
	ShaderPtr m_shaderTonemap;



	// ------------------------------------------------------------
	// General

	int m_UBOAlignment; ///< Memory alignment for UBOs (driver value)

	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<Voxelization> m_voxelization;

	gl::UniformBufferMetaInfo m_uboInfoConstant;
	BufferPtr m_uboConstant;
	gl::UniformBufferMetaInfo m_uboInfoPerFrame;
	BufferPtr m_uboPerFrame;

	std::unique_ptr<gl::PersistentRingBuffer> m_uboRing_PerObject;
	unsigned int m_perObjectUBOBindingPoint;
	unsigned int m_perObjectUBOSize;

	const gl::SamplerObject& m_samplerLinear;
	const gl::SamplerObject& m_samplerNearest;

	/// List of all shaders for convenience purposes.
	std::vector<gl::ShaderObject*> m_allShaders;
};

