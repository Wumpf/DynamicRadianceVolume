#version 450 core

// Base level is set to layer from which to pull.
layout(binding=0) uniform sampler2D CacheSpecularEnvmap;

in vec2 Texcoord;
out vec3 OutputColor;

void main()
{
	OutputColor = textureLod(CacheSpecularEnvmap, Texcoord, 0).rgb;
}