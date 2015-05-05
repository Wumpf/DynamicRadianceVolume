#version 450 core

#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

in vec3 Position;
in vec3 Normal;
in vec2 Texcoord;

layout(location = 0) out vec3 OutFlux;
layout(location = 1) out ivec2 OutPackedNormal;
layout(location = 2) out vec2 OutDepthLinSq;

uniform sampler2D DiffuseTexture;

void main()
{
	vec3 toLight = LightPosition - Position;
	float distToLight = length(toLight);
	toLight /= distToLight;

	float cosToLight = saturate(dot(-toLight, LightDirection));

	float totalSpotSteradian = PI_2 * (1.0 - LightCosHalfAngle); // https://en.wikipedia.org/wiki/Steradian#Other_properties
	float pixelSteradian = totalSpotSteradian * cosToLight / ShadowMapResolution / ShadowMapResolution; // cos(alpha) / pixel area
	float spotFalloff = ComputeSpotFalloff(cosToLight); 

	// Actual intensity for given Direction = spotFallOff * LightIntensity
	// Remember: Intensity = Flux per Steradian
	// -> total incoming flux = Intensity * PixelSteradian
	// -> total outgoing flux = Incoming Flux * "reflectivity"

	OutFlux = texture(DiffuseTexture, Texcoord).rgb * LightIntensity * (spotFalloff * pixelSteradian);
	OutPackedNormal = PackNormal16I(normalize(Normal));
	OutDepthLinSq = vec2(distToLight, distToLight * distToLight);
}