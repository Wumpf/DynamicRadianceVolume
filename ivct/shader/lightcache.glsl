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

#ifdef LIGHTCACHE_CREATION_STATS
	layout(std430, binding = 1) restrict buffer LightCacheStats
	{
		int LightCacheHashCollisionCount;
		int LightCacheActiveCount;
	};
#endif


// Infos encoded in 8bit voxel grid
#define SOLID_BIT uint(2)