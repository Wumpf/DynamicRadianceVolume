#version 450 core

#include "gbuffer.glsl"
#include "globalubos.glsl"
#include "utils.glsl"
#include "lightcache.glsl"

in vec2 Texcoord;

layout(binding = 0, r8ui) restrict writeonly uniform uimage3D VoxelVolume;

void main()
{
	// Get pixel world position.
	float depthBufferDepth = texture(GBuffer_Depth, Texcoord).r;
	if(depthBufferDepth < 0.00001)
	{
		discard;
	}
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

	ivec3 voxelPosition = ivec3((worldPosition - VoxelVolumeWorldMin) / VoxelSizeInWorld  + vec3(0.001));
	voxelPosition = clamp(voxelPosition, ivec3(0), ivec3(VoxelResolution-2));

	imageStore(VoxelVolume, voxelPosition + ivec3(0, 0, 0), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(1, 0, 0), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(1, 1, 0), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(0, 1, 0), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(0, 0, 1), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(1, 0, 1), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(1, 1, 1), uvec4(CACHE_NEEDED_BIT));
	imageStore(VoxelVolume, voxelPosition + ivec3(0, 1, 1), uvec4(CACHE_NEEDED_BIT));
}