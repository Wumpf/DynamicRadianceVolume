#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

layout(binding=3) uniform sampler2DShadow ShadowMap;

in vec2 Texcoord;
out vec3 OutputColor;

// Evaluates BRDF.
// Attention: Parameter directions are not as (often) given in theoretical notation!
vec3 BRDF(vec3 toLight, vec3 toCamera, vec3 diffuseColor)
{
	// Diffuse Term
	vec3 brdf = diffuseColor; // Omitted 1/PI!

	return brdf;
}

void main()
{
	// Get world position and normal.
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), textureLod(GBuffer_Depth, Texcoord, 0).r, 1.0f) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;
	vec3 worldNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);

	// Check if shadowed
	vec4 shadowProjection = vec4(worldPosition, 1.0) * World * LightViewProjection;
	shadowProjection.xyz /= shadowProjection.w;
	shadowProjection.xy = shadowProjection.xy * 0.5 + vec2(0.5);
	shadowProjection.z += 0.001;
	float shadowing = textureLod(ShadowMap, shadowProjection.xyz, 0);

	if(shadowing == 0.0)
		discard;

	// Direction and distance to light.
	vec3 toLight = LightPosition - worldPosition;
//	if(dot(toLight, worldNormal) > 0) // Early out if facing away from the light. 
//		discard;
	float lightDistanceSq = dot(toLight, toLight);
	toLight *= inversesqrt(lightDistanceSq);

	// Direction to camera.
	vec3 toCamera = normalize(vec3(CameraPosition - worldPosition));

	// BRDF parameters from GBuffer.
	vec3 diffuseColor = textureLod(GBuffer_Diffuse, Texcoord, 0).rgb;
	
	// Evaluate direct light.
	float cosTheta = saturate(dot(toLight, worldNormal));
	vec3 irradiance = LightIntensity * (shadowing * ComputeSpotFalloff(toLight) * cosTheta / lightDistanceSq);
	OutputColor = BRDF(toLight, toCamera, diffuseColor) * irradiance;
}