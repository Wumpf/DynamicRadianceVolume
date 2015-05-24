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


#define INDDIFFUSE_VIA_SH1
//#define INDDIFFUSE_VIA_SH2
//#define INDDIFFUSE_VIA_H 4 // 6

#define INDIRECT_SPECULAR

struct LightCacheEntry
{
	vec3 Position; // Consider storing packed identifier!
	float _padding0;


#if defined(INDDIFFUSE_VIA_SH1) || defined(INDDIFFUSE_VIA_SH2)
	// Irradiance via SH (band, coefficient)
	// Consider packing!
	vec3 SH1neg1;
	float SH00_r;
	vec3 SH10;
	float SH00_g;
	vec3 SH1pos1;
	float SH00_b;

	#ifdef INDDIFFUSE_VIA_SH2
	vec3 SH2neg2;
	float SH20_r;
	vec3 SH2neg1;
	float SH20_g;
	vec3 SH2pos1;
	float SH20_b;
	vec3 SH2pos2;
	float _padding1;
	#endif

#elif defined(INDDIFFUSE_VIA_H)
	// Irradiance via H basis - in "Cache Local View Space"
	vec3 irradianceH1;
	float irradianceH4r;
	vec3 irradianceH2;
	float irradianceH4g;
	vec3 irradianceH3;
	float irradianceH4b;
	#if INDDIFFUSE_VIA_H > 4
	vec3 irradianceH5;
	float _padding1;
	vec3 irradianceH6;
	float _padding2;
	#endif
#endif

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

mat3 ComputeLocalViewSpace(vec3 worldPosition)
{
	mat3 localViewSpace;
	localViewSpace[2] = normalize(CameraPosition - worldPosition); // Z
	localViewSpace[0] = normalize(vec3(localViewSpace[2].z, 0.0, -localViewSpace[2].x)); // X
	localViewSpace[1] = cross(localViewSpace[2], localViewSpace[0]); // Y


	return localViewSpace;
}

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
#endif


#define LIGHTING_THREADS_PER_GROUP 512

// Infos encoded in 8bit voxel grid
#define SOLID_BIT uint(2)