#version 450 core

in vec2 Texcoord;

out vec3 OutputColor;

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightcache.glsl"

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


	vec3 totalNormal = vec3(0.0);


	int cachePosition[8];
	int cacheIndex[8];
	GetCacheHashNeighbors(worldPosition, cachePosition, cacheIndex);

	int numNonZero = 0;

	for(int i=0; i<8; ++i)
	{
		int actualCachePosition = LightCacheEntries[cacheIndex[i]].Position;

		while(actualCachePosition != cachePosition[i]+1 && actualCachePosition != 0)
		{
			cacheIndex[i] = (cacheIndex[i] + 1) % MaxNumLightCaches;
			actualCachePosition = LightCacheEntries[cacheIndex[i]].Position;
		}

		if(actualCachePosition != 0)
		{
			totalNormal += LightCacheEntries[cacheIndex[i]].Normal;
			++numNonZero;
		}
	}


	//OutputColor = numNonZero == 0 ? vec3(1,0,1) : vec3(numNonZero / 8.0);
	//OutputColor =  UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg) * 0.5 + vec3(0.5);
	OutputColor = normalize(totalNormal) * 0.5 + vec3(0.5);
	//OutputColor = vec3((cachePositionInt+1) - LightCacheEntries[cacheIndex].Position);
}