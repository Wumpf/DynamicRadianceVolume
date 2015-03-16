#version 450 core
#extension GL_NV_shader_atomic_float : require

#include "globalubos.glsl"
#include "lightcache.glsl"
#include "gbuffer.glsl"
#include "utils.glsl"

layout(location = 0) in vec3 gs_out_VoxelPos;
layout(location = 1) in vec3 gs_out_WorldPos;
layout(location = 2) in vec3 gs_out_Normal;
layout(location = 3) in vec2 gs_out_Texcoord;

//layout(binding = 0, r8) restrict writeonly uniform image3D VoxelVolume;

//layout(location = 0, index = 0) out vec4 FragColor;

void main()
{
	// Check if in frustum.
	vec4 projectedWorldPos = vec4(gs_out_WorldPos, 1.0) * ViewProjection;

	if(-projectedWorldPos.w <= projectedWorldPos.x && projectedWorldPos.x <= projectedWorldPos.w &&
	   -projectedWorldPos.w <= projectedWorldPos.y && projectedWorldPos.y <= projectedWorldPos.w &&
	   0.0 <= projectedWorldPos.z && projectedWorldPos.z <= projectedWorldPos.w)
	{
		int cachePositionInt;
		int cacheIndex = GetCacheHash(gs_out_WorldPos, cachePositionInt);

		// If no position was written to this cache, change position.
		int oldCachePosition = atomicCompSwap(LightCacheEntries[cacheIndex].Position, 0, cachePositionInt+1);
	
		// Already used and not this position
		while(oldCachePosition != cachePositionInt+1 && oldCachePosition != 0)
		{
			cacheIndex = (cacheIndex + 1) % MaxNumLightCaches;
			oldCachePosition = atomicCompSwap(LightCacheEntries[cacheIndex].Position, 0, cachePositionInt+1);

		#ifdef COUNT_LIGHTCACHE_HASH_COLLISIONS
			atomicAdd(LightCacheHashCollisionCount, 1);
		#endif
		}

		// Missuse collision counter as voxel counter
		/*if(oldCachePosition == 0)
		{
			atomicAdd(LightCacheHashCollisionCount, 1);
		}*/

		atomicAdd(LightCacheEntries[cacheIndex].Normal.x, gs_out_Normal.x);
		atomicAdd(LightCacheEntries[cacheIndex].Normal.y, gs_out_Normal.y);
		atomicAdd(LightCacheEntries[cacheIndex].Normal.z, gs_out_Normal.z);
	}

	//ivec3 voxelVolumeSize = imageSize(VoxelVolume);
	//imageStore(VoxelVolume, ivec3(gs_out_VoxelPos * voxelVolumeSize), vec4(1.0));

	//FragColor = vec4(1.0);
}