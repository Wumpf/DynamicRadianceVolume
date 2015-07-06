#version 450 core

#include "../globalubos.glsl"

#include "../utils.glsl"
#include "../lightingfunctions.glsl"
#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "../lightcache.glsl"

layout(binding=6) uniform sampler2D CacheSpecularEnvmap;

in VertexOutput
{
	vec3 LocalPosition;
	vec3 WorldPosition;
	flat int InstanceID;
} In;

out vec3 OutputColor;

void main()
{
	vec3 worldNormal = normalize(In.LocalPosition);
	int cacheAddress = In.InstanceID;
	
	// SPECULAR
	vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(In.WorldPosition);
	vec2 specEnvLookupCoord = HemisphericalProjection(cacheViewNormal);
	vec2 cacheSpecularEnvmapOffset = vec2(cacheAddress % SpecularEnvmapNumCachesPerDimension, cacheAddress / SpecularEnvmapNumCachesPerDimension);
	vec2 cacheSpecularEnvmapTex = (specEnvLookupCoord + cacheSpecularEnvmapOffset) * SpecularEnvmapPerCacheSize_Texcoord;
	OutputColor = textureLod(CacheSpecularEnvmap, cacheSpecularEnvmapTex, 0.0).rgb;

	// DIFFUSE
	OutputColor = SampleCacheIrradiance(cacheAddress, worldNormal);

	// NORMAL
	//OutputColor = worldNormal * 0.5 + vec3(0.5);
}