#pragma once

#include <memory>
#include <vector>
#include <ei/vector.hpp>
#include "camera/camera.hpp"
#include "../shaderreload/autoreloadshaderptr.hpp"

#include <glhelper/shaderdatametainfo.hpp>
#include <algorithm>


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
typedef std::unique_ptr<gl::Buffer> BufferPtr;
typedef std::unique_ptr<gl::FramebufferObject> FramebufferObjectPtr;

class Renderer
{
public:
	Renderer(const std::shared_ptr<const Scene>& scene, const ei::UVec2& resolution);
	~Renderer();

	enum class Mode
	{
		RSM_BRUTEFORCE = 0,
		DYN_RADIANCE_VOLUME = 1,

		GBUFFER_DEBUG = 3,
		DIRECTONLY,
		DIRECTONLY_CACHE,
		VOXELVIS,
		AMBIENTOCCLUSION
	};

	void SetMode(Mode mode) { m_mode = mode; }
	Mode GetMode() const { return m_mode; }


	/// Activate/deactivates casting of indirect shadows.
	void SetIndirectShadow(bool active)		{ m_indirectShadow = active; ReloadSettingDependentCacheShader(); }
	bool GetIndirectShadow() const			{ return m_indirectShadow; }
	/// Activates/deactivates glossy indirect lighting.
	void SetIndirectSpecular(bool active)	{ m_indirectSpecular = active; ReloadSettingDependentCacheShader(); }
	bool GetIndirectSpecular() const		{ return m_indirectSpecular; }

	/// Sets size of the per cache specular env map in pixel.
	///
	/// \attention Needs to be a power of two!
	/// This can lead to a resize of the SpecularEnvMap texture atlas, depending on the MaxCacheCount setting.
	/// Usually you should use very small values ranging from 8-32.
	void SetPerCacheSpecularEnvMapSize(unsigned int specularEnvmapPerCacheSize);
	unsigned int GetPerCacheSpecularEnvMapSize() const { return m_specularEnvmapPerCacheSize; }

	/// Sets maximum number of hole fill level passes.
	///
	/// Number determines max level from which info is retrieved. Can't be higher than log2(GetPerCacheSpecularEnvMapSize()).
	/// 0 Means no hole fill. Recomend is log2(GetPerCacheSpecularEnvMapSize())/2
	void SetSpecularEnvMapHoleFillLevel(unsigned int holeFillLevel) { m_specularEnvmapMaxFillHolesLevel = std::min(holeFillLevel, static_cast<unsigned int>(log2(m_specularEnvmapPerCacheSize))); }
	unsigned int GetSpecularEnvMapHoleFillLevel() const { return m_specularEnvmapMaxFillHolesLevel; }

	/// Sets maximum cache count. Will print warning to log if given value can not be achieved.
	void SetMaxCacheCount(unsigned int maxCacheCount)	{ m_maxNumLightCaches = maxCacheCount; ReallocateCacheData(); UpdateConstantUBO(); }
	unsigned int GetMaxCacheCount() const				{ return m_maxNumLightCaches; }


	/// Should be called on screen resize.
	void OnScreenResize(const ei::UVec2& newResolution);

	void SetScene(const std::shared_ptr<const Scene>& scene);
	const std::shared_ptr<const Scene>& GetScene() const { return m_scene; }

	void Draw(const Camera& camera);

	/// Saves HDR buffer to a pfm file.
	void SaveToPFM(const std::string& filename) const;

	/// Enables/Disables tracking of current light cache count.
	/// \attention Tracking can seriously hurt the overall performance! 
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

	void LoadAllShaders();
	/// Reloads several cache related shaders that have macros depending on the current configuration.
	void ReloadSettingDependentCacheShader();

	/// Reallocates cache buffer and cache specular envmap according to the current configuration.
	///
	/// If cache specular envmap would exceed maximum size, number of caches will be reduced.
	/// \attention You might also want to call UpdateConstantUBO()
	void ReallocateCacheData();

	unsigned int RoundSizeToUBOAlignment(unsigned int size) { return size + (m_UBOAlignment - size % m_UBOAlignment) % m_UBOAlignment; }

	void UpdateConstantUBO(); 
	void UpdatePerFrameUBO(const Camera& camera);

	void UpdatePerObjectUBORingBuffer();
	void PrepareLights();

	void PrepareSpecularCacheEnvmaps();

	/// Binds gbuffer with nearest sampler with bindings according to 
	void BindGBuffer();

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


	void ConeTraceAO();

	/// Draws scene, mesh by mesh.
	///
	/// Does set VAO, VBO and index buffers but nothing else. No culling!
	void DrawScene(bool setTextures);


	// ------------------------------------------------------------
	// Indirect lighting

	std::unique_ptr<gl::Texture3D> m_lightCacheAddressVolume;

	unsigned int m_maxNumLightCaches;
	bool m_readLightCacheCount;
	unsigned int m_lastNumLightCaches;
	BufferPtr m_lightCacheBuffer;
	BufferPtr m_lightCacheCounter;

	bool m_indirectSpecular;
	Texture2DPtr m_specularCacheEnvmap;
	std::vector<std::shared_ptr<gl::FramebufferObject>> m_specularCacheEnvmapFBOs;
	unsigned int m_specularEnvmapPerCacheSize; ///< Resolution of specular map per cache
	unsigned int m_specularEnvmapMaxFillHolesLevel; ///< Zero means no push pull

	bool m_indirectShadow;

	AutoReloadShaderPtr m_shaderCacheGather;
	AutoReloadShaderPtr m_shaderCacheApply;
	AutoReloadShaderPtr m_shaderLightCachePrepare;
	AutoReloadShaderPtr m_shaderLightCachesDirect;
	AutoReloadShaderPtr m_shaderLightCachesRSM;

	AutoReloadShaderPtr m_shaderSpecularEnvmapMipMap;
	AutoReloadShaderPtr m_shaderSpecularEnvmapFillHoles;

	AutoReloadShaderPtr m_shaderConeTraceAO;


	// ------------------------------------------------------------
	// Direct lighting / RSM

	AutoReloadShaderPtr m_shaderDebugGBuffer;
	AutoReloadShaderPtr m_shaderFillGBuffer;
	AutoReloadShaderPtr m_shaderFillRSM;

	AutoReloadShaderPtr m_shaderDeferredDirectLighting_Spot;

	gl::UniformBufferMetaInfo m_uboInfoSpotLight;
	std::unique_ptr<gl::PersistentRingBuffer> m_uboRing_SpotLight;


	Texture2DPtr m_GBuffer_diffuse;
	Texture2DPtr m_GBuffer_roughnessMetallic;
	Texture2DPtr m_GBuffer_normal;
	Texture2DPtr m_GBuffer_depth;
	FramebufferObjectPtr m_GBuffer;
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
		gl::Texture2D* depthLinSq;
		gl::Texture2D* depthBuffer;
		gl::FramebufferObject* fbo;

	private:
		void DeInit();
	};
	std::vector<ShadowMap> m_shadowMaps; ///< A shadowmap for every light.

	Texture2DPtr m_HDRBackbufferTexture;
	FramebufferObjectPtr m_HDRBackbuffer;
	AutoReloadShaderPtr m_shaderTonemap;

	AutoReloadShaderPtr m_shaderIndirectLightingBruteForceRSM;

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

	const gl::SamplerObject& m_samplerLinearRepeat;
	const gl::SamplerObject& m_samplerLinearClamp;
	const gl::SamplerObject& m_samplerNearest;
	const gl::SamplerObject& m_samplerShadow;
};

