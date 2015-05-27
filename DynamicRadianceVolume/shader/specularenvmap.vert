#version 450 core

#include "globalubos.glsl"
#include "lightcache.glsl"

// input
layout(location = 0) in vec2 inPosition;

// output
layout(location = 0) out vec2 Texcoord;

void main()
{	
	gl_Position.xy = inPosition;
	gl_Position.zw = vec2(1.0, 1.0);
	
	// Move triangle down as far as possible - should reduce written area, thus improving perf.
	float numUsedRows = ceil(float(TotalLightCacheCount) / SpecularEnvmapNumCachesPerDimension);
	float rowPercentage = numUsedRows / SpecularEnvmapNumCachesPerDimension;
	gl_Position.y -= 2.0 - rowPercentage * 2.0;

	Texcoord = gl_Position.xy*0.5 + 0.5;
}