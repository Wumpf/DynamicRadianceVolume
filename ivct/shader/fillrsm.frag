#version 450 core

#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

in vec3 Position;
in vec3 Normal;
in vec2 Texcoord;

layout(location = 0) out vec3 OutFlux;
layout(location = 1) out ivec2 OutPackedNormal;

uniform sampler2D DiffuseTexture;

void main()
{
	vec3 toLight = normalize(LightPosition - Position);
	float cosToLight = dot(-toLight, LightDirection);
	float pixelSteradian = cosToLight / ShadowMapResolution / ShadowMapResolution; // cos(alpha) / pixel area
	float spotFalloff = ComputeSpotFalloff(cosToLight); 

	// Actual intensity for given Direction = spotFallOff * LightIntensity
	// Remember: Intensity = Flux per Steradian
	// -> total incoming flux = Intensity * PixelSteradian
	// -> total outgoing flux = Incoming Flux * "reflectivity"

	OutFlux = texture(DiffuseTexture, Texcoord).rgb * LightIntensity * (spotFalloff * pixelSteradian);
	OutPackedNormal = PackNormal16I(normalize(Normal));
}