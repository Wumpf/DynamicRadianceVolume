#version 450 core

// Options:
//#define ALPHATESTING <DesiredAlphaThreshhold>

#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

in vec3 Position;
in vec3 Normal;
in vec3 Tangent;
in float BitangentHandedness;
in vec2 Texcoord;

layout(location = 0) out vec3 OutFlux;
layout(location = 1) out ivec2 OutPackedNormal;
layout(location = 2) out vec2 OutDepthLinSq;

layout(binding = 0) uniform sampler2D BaseColorTexture;
layout(binding = 1) uniform sampler2D Normalmap;
layout(binding = 2) uniform sampler2D RoughnessMetalic;

void main()
{
	vec4 baseColor = texture(BaseColorTexture, Texcoord);
#ifdef ALPHATESTING
	if(baseColor.a < 0.1)
		discard;
#endif

	vec3 toLight = LightPosition - Position;
	float distToLight = length(toLight);
	toLight /= distToLight;

	float cosToLight = saturate(dot(-toLight, LightDirection));

	float totalSpotSteradian = PI_2 * (1.0 - LightCosHalfAngle); // https://en.wikipedia.org/wiki/Steradian#Other_properties
	float pixelSteradian = totalSpotSteradian * cosToLight / RSMRenderResolution / RSMRenderResolution; // cos(alpha) / pixel area
	float spotFalloff = ComputeSpotFalloff(cosToLight); 

	// Actual intensity for given Direction = spotFallOff * LightIntensity
	// Remember: Intensity = Flux per Steradian
	// -> total incoming flux = Intensity * PixelSteradian
	// -> total outgoing flux = Incoming Flux * "reflectivity"

	OutFlux = baseColor.rgb * LightIntensity * (spotFalloff * pixelSteradian / PI); // /PI is preponed for later intensity calculation
	OutDepthLinSq = vec2(distToLight, distToLight * distToLight);


	// Normal with normalmapping.
	vec3 normalMapNormal = texture(Normalmap, Texcoord).xyz;
	normalMapNormal.xy = normalMapNormal.xy * 2.0 - 1.0;
	normalMapNormal = normalize(normalMapNormal);

	vec3 vnormal = normalize(Normal);
	vec3 vtangent = normalize(Tangent.xyz);
	vec3 vbitangent = cross(vnormal, vtangent) * BitangentHandedness;
	vec3 finalNormal = mat3(vtangent, vbitangent, vnormal) * normalMapNormal;

	OutPackedNormal = PackNormal16I(normalize(finalNormal));
}