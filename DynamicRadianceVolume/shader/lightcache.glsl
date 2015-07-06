// Options with effects on multiple shaders - most are wired as compile option into the program.

// Diffuse lighting mode.
//#define INDDIFFUSE_VIA_SH1
//#define INDDIFFUSE_VIA_SH2
//#define INDDIFFUSE_VIA_H 4 // 6

// Enable cascade transitions.
//#define ADDRESSVOL_CASCADE_TRANSITIONS

// Enable specular lighting.
//#define INDIRECT_SPECULAR


#define LIGHTCACHEMODE_CREATE 0
#define LIGHTCACHEMODE_LIGHT 1
#define LIGHTCACHEMODE_APPLY 2

#if LIGHTCACHEMODE == LIGHTCACHEMODE_CREATE
	#define LIGHTCACHE_BUFFER_MODIFIER restrict writeonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict coherent
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_LIGHT
	#define LIGHTCACHE_BUFFER_MODIFIER restrict
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#elif LIGHTCACHEMODE == LIGHTCACHEMODE_APPLY
	#define LIGHTCACHE_BUFFER_MODIFIER restrict readonly
	#define LIGHTCACHE_COUNTER_MODIFIER restrict readonly
#else
	#error "Please specify LIGHTCACHEMODE!"
#endif


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




#define LIGHTING_THREADS_PER_GROUP 512


mat3 ComputeLocalViewSpace(vec3 worldPosition)
{
	mat3 localViewSpace;
	localViewSpace[2] = normalize(CameraPosition - worldPosition); // Z
	localViewSpace[0] = normalize(vec3(localViewSpace[2].z, 0.0, -localViewSpace[2].x)); // X
	localViewSpace[1] = cross(localViewSpace[2], localViewSpace[0]); // Y

	return localViewSpace;
}

// Computes address volume cascade for a given worldPosition. If none fits, returns highest cascade (NumAddressVolumeCascades-1)
int ComputeAddressVolumeCascade(vec3 worldPosition)
{
	int addressVolumeCascade = 0;
	for(; addressVolumeCascade<NumAddressVolumeCascades-1; ++addressVolumeCascade)
	{
		if(all(lessThanEqual(worldPosition, AddressVolumeCascades[addressVolumeCascade].DecisionMax) &&
			greaterThanEqual(worldPosition, AddressVolumeCascades[addressVolumeCascade].DecisionMin)))
		{
			break;
		}
	}

	return addressVolumeCascade;
}

/// Returns 0.0 if its not in transition area. 1.0 max transition to NEXT cascade.
float ComputeAddressVolumeCascadeTransition(vec3 worldPosition, int addressVolumeCascade)
{
	vec3 distToMax = AddressVolumeCascades[addressVolumeCascade].DecisionMax - worldPosition;
	vec3 distToMin = worldPosition - AddressVolumeCascades[addressVolumeCascade].DecisionMin;

	float minDist = min(min(min(distToMax.x, distToMax.y), distToMax.z),
						min(min(distToMin.x, distToMin.y), distToMin.z));

	return clamp(1.0 - minDist / (AddressVolumeCascades[addressVolumeCascade].WorldVoxelSize * CAVTransitionZoneSize), 0.0, 1.0);
}

#if LIGHTCACHEMODE == LIGHTCACHEMODE_APPLY
	vec3 SampleCacheIrradiance(uint cacheAddress, vec3 worldNormal)
	{
		// IRRADIANCE VIA SH
	#if defined(INDDIFFUSE_VIA_SH1) || defined(INDDIFFUSE_VIA_SH2)
		// Band 0
		vec3 irradiance = vec3(LightCacheEntries[cacheAddress].SH00_r,
								LightCacheEntries[cacheAddress].SH00_g,
								LightCacheEntries[cacheAddress].SH00_b) * ShEvaFactor0;

		// Band 1
		irradiance -= LightCacheEntries[cacheAddress].SH1neg1 * (ShEvaFactor1 * worldNormal.y);
		irradiance += LightCacheEntries[cacheAddress].SH10 * (ShEvaFactor1 * worldNormal.z);
		irradiance -= LightCacheEntries[cacheAddress].SH1pos1 * (ShEvaFactor1 * worldNormal.x);

		// Band 2
		#ifdef INDDIFFUSE_VIA_SH2
		irradiance -= LightCacheEntries[cacheAddress].SH2neg2 * (ShEvaFactor2n2_p1_n1 * worldNormal.x * worldNormal.y);
		irradiance += LightCacheEntries[cacheAddress].SH2neg1 * (ShEvaFactor2n2_p1_n1 * worldNormal.y * worldNormal.z);
		irradiance += vec3(LightCacheEntries[cacheAddress].SH20_r,
								LightCacheEntries[cacheAddress].SH20_g,
								LightCacheEntries[cacheAddress].SH20_b) * (ShEvaFactor20 * (worldNormal.z * worldNormal.z * 3.0 - 1.0));
		irradiance += LightCacheEntries[cacheAddress].SH2pos1 * (ShEvaFactor2n2_p1_n1 * worldNormal.x * worldNormal.z);
		irradiance += LightCacheEntries[cacheAddress].SH2pos2 * (ShEvaFactor2p2 * (worldNormal.x * worldNormal.x - worldNormal.y * worldNormal.y));	
		#endif

		// -----------------------------------------------
		// IRRADIANCE VIA H-Basis
	/*#elif defined(INDDIFFUSE_VIA_H)
		irradiance  = LightCacheEntries[cacheAddress].irradianceH1 * factor0;
		irradiance -= LightCacheEntries[cacheAddress].irradianceH2 * factor1 * cacheViewNormal.y;
		irradiance += LightCacheEntries[cacheAddress].irradianceH3 * factor1 * (2.0 * cacheViewNormal.z - 1.0);
		irradiance -= vec3(LightCacheEntries[cacheAddress].irradianceH4r,
								LightCacheEntries[cacheAddress].irradianceH4g,
								LightCacheEntries[cacheAddress].irradianceH4b) * (factor1 * cacheViewNormal.x);
		#if INDDIFFUSE_VIA_H > 4
		irradiance += LightCacheEntries[cacheAddress].irradianceH5 * (factor2 * cacheViewNormal.x * cacheViewNormal.y);
		irradiance += LightCacheEntries[cacheAddress].irradianceH6 * (factor2 * (cacheViewNormal.x * cacheViewNormal.x - cacheViewNormal.y * cacheViewNormal.y) * 0.5);
		#endif*/

		// Negative irradiance values are not meaningful (may happen due to SH overshooting)
		irradiance = max(irradiance, vec3(0.0));

		return irradiance;
	#endif

		return vec3(0.0);
	}
#endif