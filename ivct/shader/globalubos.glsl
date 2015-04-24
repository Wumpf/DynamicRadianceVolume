// UBO for values that change very rarely (max every few seconds or even minutes)
layout(binding = 0, shared) uniform Constant
{
	ivec2 BackbufferResolution;
	int VoxelResolution;
	int AddressVolumeResolution;
	uint MaxNumLightCaches;
};

// UBO for values that change once every frame.
layout(binding = 1, shared) uniform PerFrame
{
	mat4 Projection;
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
	vec3 VoxelSizeInWorld; 			// (VolumeWorldMax - VolumeWorldMin) / VoxelResolution
	vec3 AddressVolumeVoxelSize; 	// (VolumeWorldMax - VolumeWorldMin) / AddressGridResolution
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
};
