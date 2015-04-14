// UBO for values that change very rarely (max every few seconds or even minutes)
layout(binding = 0, shared) uniform Constant
{
	ivec2 BackbufferResolution;
	int VoxelResolution;
	uint MaxNumLightCaches;
};

// UBO for values that change once every frame.
layout(binding = 1, shared) uniform PerFrame
{
	mat4 Projection;
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec3 CameraPosition;
	vec3 CameraDirection;

	vec3 VoxelVolumeWorldMin; // World min coordinate of voxel volume (currently assumed to be scene-static)
	vec3 VoxelVolumeWorldMax; // || max
	vec3 VoxelSizeInWorld; // (VoxelVolumeWorldMax - VoxelVolumeWorldMin) / VoxelResolution
};

// UBO for values that change with each object
layout(binding = 2, shared) uniform PerObject
{
	mat4 World;
};

// UBO for a single spot light. Likely to be changed in something more general.
layout(binding = 3, std140) uniform SpotLight
{
	vec3 LightIntensity;
	vec3 LightPosition;
	vec3 LightDirection;
	float LightCosHalfAngle;
};
