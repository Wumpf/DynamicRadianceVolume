#version 450 core

// Options:
//#define ALPHATESTING <DesiredAlphaThreshhold>

#include "utils.glsl"

in vec3 Normal;
in vec3 Tangent;
in float BitangentHandedness;
in vec2 Texcoord;

layout(binding = 0) uniform sampler2D BaseColorTexture;
layout(binding = 1) uniform sampler2D Normalmap;
layout(binding = 2) uniform sampler2D RoughnessMetalic;

layout(location = 0) out vec3 OutBaseColor;
layout(location = 1) out ivec2 OutPackedNormal;
layout(location = 2) out vec2 OutRoughnessMetallic;

void main()
{
	vec4 baseColor = texture(BaseColorTexture, Texcoord);
#ifdef ALPHATESTING
	if(baseColor.a < 0.1)
		discard;
#endif
	OutBaseColor = baseColor.rgb;

	// Normal with normalmapping.
	vec3 normalMapNormal = texture(Normalmap, Texcoord).xyz;
	normalMapNormal.xy = normalMapNormal.xy * 2.0 - 1.0;
	normalMapNormal = normalize(normalMapNormal);
	//	normalMapNormal = vec3(0.0, 0.0, 1.0); // Offswitch
	vec3 vnormal = normalize(Normal);
	vec3 vtangent = normalize(Tangent.xyz);
	vec3 vbitangent = cross(vtangent, vnormal) * BitangentHandedness;
	vec3 finalNormal = mat3(vtangent, vbitangent, vnormal) * normalMapNormal;
	OutPackedNormal = PackNormal16I(normalize(finalNormal));


	OutRoughnessMetallic = texture(RoughnessMetalic, Texcoord).rg;
}