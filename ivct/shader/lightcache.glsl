#define LIGHTCACHEMODE_CREATE 0
#define LIGHTCACHEMODE_LIGHT 1
#define LIGHTCACHEMODE_APPLY 2

#if LIGHTCACHEMODE == LIGHTCACHEMODE_CREATE
	#define LIGHTCACHE_BUFFER_MODIFIER restrict writeonly
	#define LIGHTCACHE_HASHMAP_MODIFIER restrict
	#define LIGHTCACHE_COUNTER_MODIFIER restrict coherent
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_LIGHT
	#define LIGHTCACHE_BUFFER_MODIFIER restrict
	#define LIGHTCACHE_HASHMAP_MODIFIER restrict readonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_APPLY
	#define LIGHTCACHE_BUFFER_MODIFIER restrict readonly
	#define LIGHTCACHE_HASHMAP_MODIFIER restrict readonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#else
	#error "Please specify LIGHTCACHEMODE!"
#endif



struct LightCacheEntry
{
	vec3 Position; // Consider storing packed identifier!
	float _padding1;

	// We do not actually want to save irradiance.
	// Will add sth. better later.
	vec3 Irradiance;
	ivec2 PixelPos;
};

layout(std430, binding = 0) LIGHTCACHE_BUFFER_MODIFIER buffer LightCacheBuffer
{
	LightCacheEntry[] LightCacheEntries;
};

layout(std430, binding = 1) LIGHTCACHE_COUNTER_MODIFIER buffer LightCacheCounter
{
	uint NumCacheLightingThreadGroupsX; // Should be (TotalLightCacheCount + LIGHTING_THREADS_PER_GROUP - 1) / LIGHTING_THREADS_PER_GROUP
    uint NumCacheLightingThreadGroupsY; // Should be 1
    uint NumCacheLightingThreadGroupsZ; // Should be 1

    int TotalLightCacheCount;
};


// Alternative adress storing
/*struct LightCacheHashEntry
{
	uint Identifier;
	uint Address;
};

layout(std430, binding = 2) LIGHTCACHE_HASHMAP_MODIFIER buffer LightCacheHashMap
{
	LightCacheHashEntry[] LightCacheHashEntries;
};*/

// Grid address storage
#if LIGHTCACHEMODE == LIGHTCACHEMODE_CREATE
	layout(binding = 0, r32ui) restrict coherent uniform uimage3D VoxelAddressVolume;
#else
	layout(binding = 3) uniform usampler3D VoxelAddressVolume;
#endif
const float AddressVolumeVoxelSize = 0.21f; // TODO


#define LIGHTING_THREADS_PER_GROUP 512

// Infos encoded in 8bit voxel grid
#define SOLID_BIT uint(2)