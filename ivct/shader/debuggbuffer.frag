#version 450 core

in vec2 Texcoord;

out vec3 OutputColor;

#include "gbuffer.glsl"
#include "utils.glsl"

void main()
{
	//OutputColor = texture(GBuffer_Diffuse, Texcoord).rgb;
	OutputColor = texture(GBuffer_Depth, Texcoord).rrr;
	//OutputColor = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg) * 0.5 + 0.5;
}