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



int CacheHashFunction(int cachePositionInt)
{
	return cachePositionInt % MaxNumLightCaches;
}

int GetCacheHash(ivec3 voxelPosition, out int cachePositionInt)
{
	cachePositionInt = int(voxelPosition.x + (voxelPosition.y + voxelPosition.z * VoxelResolution) * VoxelResolution);
	return CacheHashFunction(cachePositionInt);
}

int GetCacheHash(vec3 worldPosition, out int cachePositionInt)
{
	ivec3 voxelPosition = ivec3((worldPosition - VoxelVolumeWorldMin) / VoxelSizeInWorld + vec3(0.5));
	return GetCacheHash(voxelPosition, cachePositionInt);
}

// Indices:
#define INDEX_000 0
#define INDEX_100 1
#define INDEX_010 2
#define INDEX_110 3
#define INDEX_001 4
#define INDEX_101 5
#define INDEX_011 6
#define INDEX_111 7
void GetCacheHashNeighbors(vec3 worldPosition, out int cachePosition[8], out int cacheHash[8])
{
	ivec3 cachePositionVec3Grid = ivec3((worldPosition - VoxelVolumeWorldMin) / VoxelSizeInWorld + vec3(0.001));

	cachePosition[INDEX_000] = int(cachePositionVec3Grid.x + (cachePositionVec3Grid.y + cachePositionVec3Grid.z * VoxelResolution) * VoxelResolution);
	cachePosition[INDEX_100] = cachePosition[INDEX_000] + 1;
	cachePosition[INDEX_010] = cachePosition[INDEX_000] + VoxelResolution;
	cachePosition[INDEX_110] = cachePosition[INDEX_010] + 1;
	cachePosition[INDEX_001] = cachePosition[INDEX_000] + VoxelResolution * VoxelResolution;
	cachePosition[INDEX_101] = cachePosition[INDEX_001] + 1;
	cachePosition[INDEX_011] = cachePosition[INDEX_001] + VoxelResolution;
	cachePosition[INDEX_111] = cachePosition[INDEX_011] + 1;

	for(int i=0; i<8; ++i)
		cacheHash[i] = CacheHashFunction(cachePosition[i]);
}