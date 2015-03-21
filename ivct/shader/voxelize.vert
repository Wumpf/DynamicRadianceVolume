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
	vs_out_Normal = inNormal;
	vs_out_Texcoord = inTexcoord;

	gl_Position = vec4((inPosition - VoxelVolumeWorldMin) /
	                (VoxelVolumeWorldMax - VoxelVolumeWorldMin) * 2.0 - vec3(1.0), 1.0);
}