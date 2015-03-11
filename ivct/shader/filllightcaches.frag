#version 450 core
#extension GL_NV_shader_atomic_float : require

in vec2 Texcoord;

out int OutputCacheIndex;

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightcache.glsl"


/*int SpatialHash(vec3 gridCoord, int hashRange)	// gridCoord must be [0, 1]
{
	vec4 n = vec4(gridCoord, gridCoord.x + gridCoord.y - gridCoord.z) * 4194304.0;

	const vec4 q = vec4(   1225.0,    1585.0,    2457.0,    2098.0);
	const vec4 r = vec4(   1112.0,     367.0,      92.0,     265.0);
	const vec4 a = vec4(   3423.0,    2646.0,    1707.0,    1999.0);
	const vec4 m = vec4(4194287.0, 4194277.0, 4194191.0, 4194167.0);

	vec4 beta = floor(n / q);
	vec4 p = a * (n - beta * q) - beta * r;
	beta = (sign(-p) + 1.0) * 0.5 * m;
	n = (p + beta);

	return int(floor( fract(dot(n / m, vec4(1.0, -1.0, 1.0, -1.0))) * hashRange));
}*/

void GetCachePosition(vec3 worldPosition, out int cachePositionInt)
{
	//cachePositionVec3 = floor(worldPosition / CacheWorldSize + vec3(0.5));
	vec3 cachePositionVec3Grid = floor((worldPosition - CacheGridMin + vec3(0.001)) / CacheWorldSize);
	cachePositionInt = int(cachePositionVec3Grid.x + cachePositionVec3Grid.y * CacheGridStrideX + cachePositionVec3Grid.z * CacheGridStrideXY);
}

void main()
{
	// Get pixel world position.
	float depthBufferDepth = texture(GBuffer_Depth, Texcoord).r;
	if(depthBufferDepth < 0.00001)
	{
		OutputCacheIndex = -1;
		return;
	}
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0f) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

	// Get cache position and hash.
	int cachePositionInt;
	GetCachePosition(worldPosition, cachePositionInt);
	// TODO: Check if different hash functions are better (some spatial hash, or cachePositionInt in morton order etc.)
	int cacheIndex = cachePositionInt % MaxNumLightCaches; //SpatialHash((cachePosition - CacheGridMin) / CacheGridSize, MaxNumLightCaches);


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

	// Missuse collision counter as hash counter
	/*if(oldCachePosition == 0)
	{
		atomicAdd(LightCacheHashCollisionCount, 1);
	}*/

	// 

	vec3 pixelNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);

	// TODO: Is there a way to reduce the amount of atomics?
	atomicAdd(LightCacheEntries[cacheIndex].Normal.x, pixelNormal.x);
	atomicAdd(LightCacheEntries[cacheIndex].Normal.y, pixelNormal.y);
	atomicAdd(LightCacheEntries[cacheIndex].Normal.z, pixelNormal.z);

	//LightCacheEntries[cacheIndex].Normal = pixelNormal;

	OutputCacheIndex = cacheIndex;
}