#version 450 core

#include "utils.glsl"

// Input = output from vertex shader.
in vec3 Normal;
in vec2 Texcoord;

layout(location = 0) out vec3 OutFlux;
layout(location = 1) out ivec2 OutPackedNormal;

uniform sampler2D DiffuseTexture;

void main()
{
	OutFlux = texture(DiffuseTexture, Texcoord).rgb;
	OutPackedNormal = PackNormal16I(normalize(Normal));
}