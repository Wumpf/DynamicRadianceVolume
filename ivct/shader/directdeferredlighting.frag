#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

in vec2 Texcoord;
out vec3 OutputColor;


layout(binding = 3, std140) uniform Light
{
	vec3 LightIntensity;
	vec3 LightPosition;
	vec3 LightDirection;
	float LightCosHalfAngle;
};

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
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), texture(GBuffer_Depth, Texcoord).r, 1.0f) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;
	vec3 worldNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);

	// Direction and distance to light.
	vec3 toLight = LightPosition - worldPosition;
//	if(dot(toLight, worldNormal) > 0) // Early out if facing away from the light. 
//		discard;
	float lightDistanceSq = dot(toLight, toLight);
	toLight *= inversesqrt(lightDistanceSq);

	// Direction to camera.
	vec3 toCamera = normalize(vec3(CameraPosition - worldPosition));

	// BRDF parameters from GBuffer.
	vec3 diffuseColor = texture(GBuffer_Diffuse, Texcoord).rgb;
	
	// Evaluate direct light.
	float spotFalloff = saturate(dot(-toLight, LightDirection) - LightCosHalfAngle) / (1.0 - LightCosHalfAngle);
	spotFalloff = sqrt(spotFalloff);
	float cosTheta = saturate(dot(toLight, worldNormal));
	vec3 irradiance = LightIntensity * (spotFalloff * cosTheta / lightDistanceSq);
	OutputColor = BRDF(toLight, toCamera, diffuseColor) * irradiance;
}