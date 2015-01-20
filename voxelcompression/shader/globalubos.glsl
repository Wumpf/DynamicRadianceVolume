// UBO for values that change very rarely (max every few seconds or even minutes)
layout(binding = 0, shared) uniform Constant
{
	vec3 VoxelSizeInWorld;
};

// UBO for values that change once every frame.
layout(binding = 1, shared) uniform PerFrame
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec3 CameraPosition;
};