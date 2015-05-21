#version 450 core

#include "utils.glsl"

layout(early_fragment_tests) in; // Force early z.

in vec3 Normal;
in vec3 Tangent;
in float BitangentHandedness;
in vec2 Texcoord;

layout(binding = 0) uniform sampler2D DiffuseTexture;
layout(binding = 1) uniform sampler2D Normalmap;
layout(binding = 2) uniform sampler2D RoughnessMetalic;

layout(location = 0) out vec3 OutDiffuse;
layout(location = 1) out ivec2 OutPackedNormal;
layout(location = 2) out vec2 OutRoughnessMetallic;

void main()
{
	OutDiffuse = texture(DiffuseTexture, Texcoord).rgb;

	// Normal with normalmapping.
	vec3 normalMapNormal = texture(Normalmap, Texcoord).xyz;
	normalMapNormal.xy = normalMapNormal.xy * 2.0 - 1.0;
//	normalMapNormal = vec3(0.0, 0.0, 1.0); // Offswitch
	vec3 vnormal = normalize(Normal);
	vec3 vtangent = normalize(Tangent.xyz);
	vec3 vbitangent = cross(vtangent, vnormal) * BitangentHandedness;
	vec3 finalNormal = mat3(vtangent, vbitangent, vnormal) * normalMapNormal;
	OutPackedNormal = PackNormal16I(normalize(finalNormal));


	OutRoughnessMetallic = texture(RoughnessMetalic, Texcoord).rg;
}