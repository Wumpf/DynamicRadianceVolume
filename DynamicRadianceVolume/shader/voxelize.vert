	#version 450 core

#include "globalubos.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

// Output
layout(location = 0) out vec3 vs_out_Normal;
layout(location = 1) out vec2 vs_out_Texcoord;

void main(void)
{
	vs_out_Normal = (vec4(inNormal, 0.0) * World).xyz;
	vs_out_Texcoord = inTexcoord;

	vec3 worldPosition = (vec4(inPosition, 1.0) * World).xyz;
	gl_Position = vec4((worldPosition - VolumeWorldMin) /
	                (VolumeWorldMax - VolumeWorldMin) * 2.0 - vec3(1.0), 1.0);
}