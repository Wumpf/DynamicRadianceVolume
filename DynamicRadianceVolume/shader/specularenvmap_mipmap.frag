#version 450 core

// Base level is set to layer from which to pull.
layout(binding=0) uniform sampler2D CacheSpecularEnvmap;

in vec2 Texcoord;
out vec3 OutputColor;

void main()
{
	vec4 reds = textureGather(CacheSpecularEnvmap, Texcoord, 0);
	vec4 greens = textureGather(CacheSpecularEnvmap, Texcoord, 1);
	vec4 blues = textureGather(CacheSpecularEnvmap, Texcoord, 2);

	OutputColor.r = reds.r + reds.g + reds.b + reds.a;
	OutputColor.g = greens.r + greens.g + greens.b + greens.a;
	OutputColor.b = blues.r + blues.g + blues.b + blues.a;

	OutputColor *= 0.25;
}