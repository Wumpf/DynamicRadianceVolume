#version 450 core

#include "globalubos.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

layout(location = 0) out vec3 WorldPosition;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 Texcoord;

void main(void)
{
	WorldPosition = (vec4(inPosition, 1.0) * World).xyz;
	Normal = (vec4(inNormal, 0.0) * World).xyz;
	Texcoord = inTexcoord;
}