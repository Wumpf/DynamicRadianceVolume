#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

layout(binding=4) uniform sampler2DShadow ShadowMap;

in vec2 Texcoord;
out vec3 OutputColor;

void main()
{
	// Get world position and normal.
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), textureLod(GBuffer_Depth, Texcoord, 0).r, 1.0f) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;
	vec3 worldNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);

	// Direction and distance to light.
	vec3 toLight = LightPosition - worldPosition;
//	if(dot(toLight, worldNormal) > 0) // Early out if facing away from the light. 
//		discard;
	float lightDistanceSq = dot(toLight, toLight);
	toLight *= inversesqrt(lightDistanceSq);
	float cosTheta = saturate(dot(toLight, worldNormal));

	// Check if shadowed - Using normal offset shadow bias http://www.dissidentlogic.com/old/#Normal%20Offset%20Shadows
	//float shadowMapTexelSize = 1.0 / RSMRenderResolution; // Could be scaled with distance. Recheck if there are problems with distant objects.
	float normalOffsetScale = ShadowNormalOffset * (1.0 - cosTheta);// * shadowMapTexelSize;
	vec4 shadowProjection;
	shadowProjection.xy = (vec4(worldPosition + normalOffsetScale * worldNormal, 1.0) * LightViewProjection).xy;
	shadowProjection.zw = (vec4(worldPosition, 1.0) * LightViewProjection).zw;
	shadowProjection.xy = shadowProjection.xy * 0.5 + vec2(0.5 * shadowProjection.w);
	shadowProjection.z += ShadowBias;
	float shadowing = textureProjLod(ShadowMap, shadowProjection, 0);

	if(shadowing == 0.0)
		discard;


	// Direction to camera.
	vec3 toCamera = normalize(vec3(CameraPosition - worldPosition));

	// BRDF parameters from GBuffer.
	vec3 baseColor = textureLod(GBuffer_Diffuse, Texcoord, 0).rgb;
	vec2 roughnessMetalic = textureLod(GBuffer_RoughnessMetalic, Texcoord, 0).rg;
	
	// Evaluate direct light.
	vec3 irradiance = LightIntensity * (shadowing * ComputeSpotFalloff(toLight) * cosTheta / lightDistanceSq);
	OutputColor = BRDF(toLight, toCamera, worldNormal, baseColor, roughnessMetalic) * irradiance;
}