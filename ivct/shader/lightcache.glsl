struct LightCacheEntry
{
	// x + y * WIDTH + z * WIDTH*HEIGHT + 1 (+1 by convetion for easy clear -> 0 is nothing)
	vec3 Normal;
	int Position;
};
layout(std430, binding = 0) restrict buffer LightCaches
{
	LightCacheEntry[] LightCacheEntries;
};

#ifdef COUNT_LIGHTCACHE_HASH_COLLISIONS
	layout(std430, binding = 1) restrict buffer LightCacheHashCollisionCounter
	{
		coherent int LightCacheHashCollisionCount;
	};
#endif

