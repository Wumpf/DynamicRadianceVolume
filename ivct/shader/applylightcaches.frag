#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

in vec2 Texcoord;

out vec3 OutputColor;

layout(binding=3) uniform usampler2D CacheAllocationMap;

void main()
{
	OutputColor = vec3(texture(CacheAllocationMap, Texcoord).rgb)*0.01;

	// Get pixel world position.
	/*float depthBufferDepth = texture(GBuffer_Depth, Texcoord).r;
	if(depthBufferDepth < 0.00001)
	{
		discard;
	}
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

*/
	//OutputColor = numNonZero == 0 ? vec3(1,0,1) : vec3(numNonZero / 8.0);
	//OutputColor =  UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg) * 0.5 + vec3(0.5);
	//OutputColor = vec3((cachePositionInt+1) - LightCacheEntries[cacheIndex].Position);
}