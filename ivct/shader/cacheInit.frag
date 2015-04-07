#version 450 core

#include "utils.glsl"

layout(early_fragment_tests) in; // Force early z.

// Input = output from vertex shader.
in vec3 Normal;
in vec2 Texcoord;

layout(location = 0) out vec3 OutDiffuse;
layout(location = 1) out ivec2 OutPackedNormal;

uniform sampler2D DiffuseTexture;

void main()
{
	OutDiffuse = texture(DiffuseTexture, Texcoord).rgb;
	OutPackedNormal = PackNormal16I(normalize(Normal));
}