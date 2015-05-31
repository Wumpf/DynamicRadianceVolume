// UBO for values that change very rarely (max every few seconds or even minutes)
layout(binding = 0, shared) uniform Constant
{
	float ShCosLobeFactor0; 	// = sqrt(PI) / 2.0;
	float ShCosLobeFactor1; 	// = -sqrt(PI / 3.0); 	// 1p1: -toVal.y; 10: toVal.z 1n1: -toVal.x
	float ShCosLobeFactor2n2_p1_n1; // = sqrt(15.0 * PI) / 8.0;  // 2n2: -toVal.x * toVal.y; 1n1: toVal.y * toVal.z; 1p1: toVal.x * toVal.z;	
	float ShCosLobeFactor20; 	// =  sqrt(5.0 * PI)  / 16.0; // * (toVal.z * toVal.z * 3.0 - 1.0);
	float ShCosLobeFactor2p2; 	// =  sqrt(15.0 * PI) / 16.0; // * (toVal.x*toVal.x - toVal.y*toVal.y);

	float ShEvaFactor0; 	// = 1.0 / (2.0 * sqrt(PI));
	float ShEvaFactor1; 	// = sqrt(3.0) / (2.0 * sqrt(PI));
	float ShEvaFactor2n2_p1_n1; // = sqrt(15.0 / (4.0 * PI)); // 2n2: -toVal.x * toVal.y; 1n1: toVal.y * toVal.z; 1p1: toVal.x * toVal.z;	
	float ShEvaFactor20; 	// = sqrt(5.0 / (16.0 * PI));
	float ShEvaFactor2p2; 	// =  sqrt(15.0 / (16.0 * PI));

	ivec2 BackbufferResolution;
	int VoxelResolution;
	int AddressVolumeResolution;
	uint MaxNumLightCaches;

	// Specular envmap is quadratic. Sizes denominate width/height.
	int SpecularEnvmapTotalSize; 				// Total size of the specular envmap texture in texel.
	int SpecularEnvmapPerCacheSize_Texel; 		// Size of per cache specular envmap in texel. Available per #define in cacheLightingRSM.comp
	float SpecularEnvmapPerCacheSize_Texcoord; 	// SpecularEnvmapPerCacheSize_Texel / SpecularEnvmapTotalSize
	int SpecularEnvmapNumCachesPerDimension; 	// SpecularEnvmapTotalSize / SpecularEnvmapPerCacheSize_Texel
};

// UBO for values that change once every frame.
layout(binding = 1, shared) uniform PerFrame
{
	mat4 Projection;
	//mat4 View;
	mat4 ViewProjection;
	mat4 InverseView;
	mat4 InverseViewProjection;
	vec3 CameraPosition;
	vec3 CameraDirection;

	// There parameters are currently quite static, but this should change later on.
	// TODO: Cascading of multiple volumes.
	// Note that both the voxel and the address volume share sizes but might not be of the same resolution!
	vec3 VolumeWorldMin; // World min coordinate of voxel volume (currently assumed to be scene-static)
	vec3 VolumeWorldMax; // || max
	float VoxelSizeInWorld; 		// Voxels are enforced to be cubic (simplifies several computations and improves quality)
	float AddressVolumeVoxelSize; 	// ||
};

// UBO for values that change with each object
layout(binding = 2, shared) uniform PerObject
{
	mat4 World;
};

// UBO for a single spot light. Likely to be changed in something more general.
layout(binding = 3, shared) uniform SpotLight
{
	vec3 LightIntensity;
	float ShadowNormalOffset;

	float ShadowBias;
	vec3 LightPosition;
	
	vec3 LightDirection;
	float LightCosHalfAngle;

	mat4 LightViewProjection;
	mat4 InverseLightViewProjection;

	int ShadowMapResolution;
	float ValAreaFactor; // NearClipWidth * NearClipHeight / (NearClipPlaneDepth² * RSMResolution * RSMResolution)

	float IndirectShadowComputationLod; 			// SHADOW_COMPUTATION_LOD 2
	float IndirectShadowComputationBlockSize;		// 1<<IndirectShadowComputationBlockSize
	
	int IndirectShadowComputationSampleInterval;  	// IndirectShadowComputationBlockSize * IndirectShadowComputationBlockSize
	float IndirectShadowComputationSuperValWidth;	// sqrt(ValAreaFactor) * SHADOW_COMPUTATION_INTERVAL_BLOCK -- The scaling factor of the "superval" used for cone tracing (scales with distance!)
	float IndirectShadowSamplingOffset; 			// = vec2(0.5 + sqrt(2.0) * IndirectShadowComputationBlockSize / 2.0);
	float IndirectShadowSamplingMinDistToSphereFactor; // = sin(minimalShadowConeAngle * 0.5)
};