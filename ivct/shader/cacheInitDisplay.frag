#version 450 core

#include "utils.glsl"

in vec2 Texcoord;

out vec3 OutputColor;

layout(binding=0) uniform sampler2D CacheInitTexture;

void main()
{
	vec4 cacheData = texture(CacheInitTexture, Texcoord);
	if(all(equal(cacheData, vec4(0.0))))
		OutputColor = vec3(0.0);
	else
		OutputColor = UnpackNormal(cacheData.xy) * 0.5 + 0.5;
}