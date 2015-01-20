#version 450 core

#include "globalubos.glsl"

// input
layout(location = 0) in vec2 Texcoord;
layout(location = 1) in vec3 RayDirection;


layout(binding = 0) uniform sampler3D VoxelScene;

// output
layout(location = 0, index = 0) out vec4 FragColor;


void main()
{
	vec3 rayDirection = normalize(RayDirection);
	FragColor = vec4(abs(rayDirection), 1.0);
}
