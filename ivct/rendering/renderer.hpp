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

	enum class Mode
	{
		RSM_BRUTEFORCE,
		RSM_CACHE,
		GBUFFER_DEBUG,
		DIRECTONLY,
		DIRECTONLY_CACHE,
		VOXELVIS
	};

	void SetMode(Mode mode) { m_mode = mode; }

	void OnScreenResize(const ei::UVec2& newResolution);

	void SetScene(const std::shared_ptr<const Scene>& scene);
	const std::shared_ptr<const Scene>& GetScene() const { return m_scene; }

	void Draw(const Camera& camera);

	/// Saves HDR buffer to a pfm file.
	void SaveToPFM(const std::string& filename) const;

	void SetReadLightCacheCount(bool trackLightCacheCreationStats);
	bool GetReadLightCacheCount() const;
	unsigned int GetLightCacheActiveCount() const;

	unsigned int GetCacheAddressVolumeSize();
	void SetCacheAdressVolumeSize(unsigned int size);

	void BindObjectUBO(unsigned int objectIndex);

	void SetExposure(float exposure);
	float GetExposure() const { return m_exposure; }

private:
	std::shared_ptr<const Scene> m_scene;

	void LoadShader();

	unsigned int RoundSizeToUBOAlignment(unsigned int size) { return size + (m_UBOAlignment - size % m_UBOAlignment) % m_UBOAlignment; }

	void UpdateConstantUBO(); 
	void UpdatePerFrameUBO(const Camera& camera);

	void UpdatePerObjectUBORingBuffer();
	void PrepareLights();

	/// Fills GBuffer.
	void DrawSceneToGBuffer();
	/// Fills shadow maps.
	void DrawShadowMaps();

	/// Draws GBuffer directly to the (hardware) backbuffer.
	void DrawGBufferDebug();

	/// Performs direct lighting for all lights.
	void DrawLights();
	
	void ApplyRSMsBruteForce();

	void OutputHDRTextureToBackbuffer();

	void GatherLightCaches();
	void ApplyLightCaches();

	/// Applies direct light to caches (mainly for debug purposes)
	void CacheLightingDirect();
	/// Applies indirect light to caches (the way its meant to be used)
	void CacheLightingRSM();

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
	ShaderPtr m_shaderLightCachesRSM;


	// ------------------------------------------------------------
	// Direct lighting / RSM

	ShaderPtr m_shaderDebugGBuffer;
	ShaderPtr m_shaderFillGBuffer;
	ShaderPtr m_shaderFillRSM;

	ShaderPtr m_shaderDeferredDirectLighting_Spot;

	gl::UniformBufferMetaInfo m_uboInfoSpotLight;
	std::unique_ptr<gl::PersistentRingBuffer> m_uboRing_SpotLight;


	Texture2DPtr m_GBuffer_diffuse;
	Texture2DPtr m_GBuffer_normal;
	Texture2DPtr m_GBuffer_depth;
	std::unique_ptr<gl::FramebufferObject> m_GBuffer;
	float m_exposure;

	struct ShadowMap
	{
		ShadowMap(ShadowMap& old);
		ShadowMap();
		~ShadowMap() { DeInit(); }

		/// Initializes all textures and the fbo with the given resolution (square)
		/// Will recreate if already initialized.
		void Init(unsigned int resolution);

		gl::Texture2D* flux;
		gl::Texture2D* normal;
		gl::Texture2D* depth;
		gl::FramebufferObject* fbo;

	private:
		void DeInit();
	};
	std::vector<ShadowMap> m_shadowMaps; ///< A shadowmap for every light.

	Texture2DPtr m_HDRBackbufferTexture;
	std::unique_ptr<gl::FramebufferObject> m_HDRBackbuffer;
	ShaderPtr m_shaderTonemap;

	ShaderPtr m_shaderIndirectLightingBruteForceRSM;

	// ------------------------------------------------------------
	// General

	int m_UBOAlignment; ///< Memory alignment for UBOs (driver value)

	Mode m_mode;

	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<Voxelization> m_voxelization;

	gl::UniformBufferMetaInfo m_uboInfoConstant;
	BufferPtr m_uboConstant;
	gl::UniformBufferMetaInfo m_uboInfoPerFrame;
	BufferPtr m_uboPerFrame;
	gl::UniformBufferMetaInfo m_uboInfoPerObject;
	std::unique_ptr<gl::PersistentRingBuffer> m_uboRing_PerObject;

	const gl::SamplerObject& m_samplerLinear;
	const gl::SamplerObject& m_samplerNearest;
	const gl::SamplerObject& m_samplerShadow;

	/// List of all shaders for convenience purposes.
	std::vector<gl::ShaderObject*> m_allShaders;
};

