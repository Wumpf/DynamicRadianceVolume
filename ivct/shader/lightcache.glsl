struct LightCacheEntry
{
	// There is a lot of room for packing! This could notably improve performance!

	vec2 PackedNormal; // Use utils.glsl UnpackNormal. Note that the source is not as accurate!
	float Roughness;
	float _padding0;

	vec3 Position; // Note that the source is not as accurate!
	float _padding1;

	// Data for Lensing-style light interpolation.
	// Likely to change!
	vec3 ProxyDiffusePosition; float _padding2;
	vec3 ProxyDiffuseFlux; float _padding3;
	vec3 ProxySpecularPosition; float _padding4;
	vec3 ProxySpecularFlux; float _padding5;
};

#define LIGHTCACHEMODE_CREATE 0
#define LIGHTCACHEMODE_LIGHT 1
#define LIGHTCACHEMODE_APPLY 2

#if LIGHTCACHEMODE == LIGHTCACHEMODE_CREATE
	#define LIGHTCACHE_BUFFER_MODIFIER restrict writeonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict coherent
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_LIGHT
	#define LIGHTCACHE_MODIFIER restrict
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_APPLY
	#define LIGHTCACHE_MODIFIER restrict readonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#else
	#error "Please specify LIGHTCACHEMODE!"
#endif



layout(std430, binding = 0) LIGHTCACHE_BUFFER_MODIFIER buffer LightCaches
{
	LightCacheEntry[] LightCacheEntries;
};

layout(std430, binding = 1) LIGHTCACHE_BUFFER_MODIFIER buffer LightCacheCounter
{
	int TotalLightCacheCount;
};


// Infos encoded in 8bit voxel grid
#define SOLID_BIT uint(2)