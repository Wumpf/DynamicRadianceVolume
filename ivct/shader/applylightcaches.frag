#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "lightcache.glsl"

in vec2 Texcoord;

out vec3 OutputColor;

layout(binding=3) uniform usampler2D CacheAllocationMap;

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

	// Get normal and material color
	vec3 worldNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg) * 0.5 + vec3(0.5);
	vec3 diffuse = texture(GBuffer_Diffuse, Texcoord).rgb;




	ivec2 textureSize = textureSize(CacheAllocationMap, 0);
	ivec2 cacheAllocationMapSampleCoord[4];
	vec2 exactGridCoord = textureSize * Texcoord;
	cacheAllocationMapSampleCoord[0] = ivec2(exactGridCoord);
	cacheAllocationMapSampleCoord[1] = cacheAllocationMapSampleCoord[0] + ivec2(1, 0);
	cacheAllocationMapSampleCoord[2] = cacheAllocationMapSampleCoord[0] + ivec2(0, 1);
	cacheAllocationMapSampleCoord[3] = cacheAllocationMapSampleCoord[0] + ivec2(1, 1);

	float weightSum = 0.0;
	OutputColor = vec3(0.0);

	for(int cacheGridSample=0; cacheGridSample<4; ++cacheGridSample)
	{
		// Retrieve cache info
		uvec4 nearestCacheAllocationEntry = texelFetch(CacheAllocationMap, cacheAllocationMapSampleCoord[cacheGridSample], 0);
		uint numCaches = nearestCacheAllocationEntry.w;
		uint cacheMemoryOffset = nearestCacheAllocationEntry.x + 
								 (nearestCacheAllocationEntry.y << 8) + 
								 (nearestCacheAllocationEntry.z << 16);

		// Apply caches.
		uint numProcessedCaches = min(128, numCaches);

		
		for(uint i=0; i<numProcessedCaches; ++i)
		{
			uint cacheIndex = cacheMemoryOffset + i;

			vec3 toCache = LightCacheEntries[cacheIndex].Position - worldPosition;
			float weight = 1.0 / (dot(toCache, toCache) + 0.01);
			weight *= saturate(dot(UnpackNormal(LightCacheEntries[cacheIndex].PackedNormal), worldNormal));

			weightSum += weight;
			OutputColor += LightCacheEntries[cacheIndex].Irradiance * weight;
		}
	}

	OutputColor /= weightSum;
	OutputColor *= diffuse;

//	OutputColor = vec3(int(numCaches) * 0.01);
	//OutputColor = vec3(int(nearestCacheAllocationEntry.y) * 0.1);

	//OutputColor = numNonZero == 0 ? vec3(1,0,1) : vec3(numNonZero / 8.0);
	//OutputColor = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg) * 0.5 + vec3(0.5);
	//OutputColor = vec3((cachePositionInt+1) - LightCacheEntries[cacheIndex].Position);
} 