// UBO for values that change very rarely (max every few seconds or even minutes)
layout(binding = 0, shared) uniform Constant
{
	float NearPlane;
	float FarPlane;

	vec3 VoxelVolumeWorldMin; // World min coordinate of voxel volume (currently assumed to be scene-static)
	vec3 VoxelVolumeWorldMax; // || max
	vec3 VoxelSizeInWorld;

	int MaxNumLightCaches;
	vec3 CacheWorldSize;
};

// UBO for values that change once every frame.
layout(binding = 1, shared) uniform PerFrame
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec3 CameraPosition;

	vec3 CacheGridMin;
	int CacheGridStrideX; // CacheGridSize.x / CacheWorldSize.x
	vec3 CacheGridSize;
	int CacheGridStrideXY; // (CacheGridSize.x / CacheWorldSize.x) * (CacheGridSize.y / CacheWorldSize.y)
};