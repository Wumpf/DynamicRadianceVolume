#version 450 core

#include "utils.glsl"

layout(binding = 0, r11f_g11f_b10f) restrict readonly uniform image2D Source;
layout(binding = 1, r11f_g11f_b10f) restrict uniform image2D Destination;

in vec2 Texcoord;

void main()
{
	ivec2 sourceSize = imageSize(Source);

	ivec2 sourceTexcoord = ivec2(Texcoord * sourceSize);
	vec3 sourceColor = imageLoad(Source, sourceTexcoord).rgb;

	// Skip if Source is zero.
	if(sourceColor.r + sourceColor.g + sourceColor.b < 0.0001)
		discard;

	ivec2 dstCoord[4];
	dstCoord[0] = sourceTexcoord * 2;
	dstCoord[1] = dstCoord[0] + ivec2(1,0);
	dstCoord[2] = dstCoord[0] + ivec2(0,1);
	dstCoord[3] = dstCoord[0] + ivec2(1,1);

	// Read destination colors.
	vec3 dstColor[4];
	for(int i=0; i<4; ++i)
		dstColor[i] = imageLoad(Destination, dstCoord[i]).rgb;

	// Fill in where color is zero.
	vec3 dstSumNew = vec3(0);
	for(int i=0; i<4; ++i)
	{
		if(dstColor[i].r+dstColor[i].g+dstColor[i].b == 0.0)
			dstColor[i] = sourceColor;
		dstSumNew += dstColor[i];
	}

	// Currently we added energy.
	// But we want to keep the same energy as in Source on average!
	dstSumNew += 0.00001; // Prevent division with zero (might happen for single colored surfaces!)
	vec3 energyNormalization = 4.0 * sourceColor / dstSumNew;
//	float energyNormalization = GetLuminance(4.0 * sourceColor / dstSumNew);

	for(int i=0; i<4; ++i)
	{
		imageStore(Destination, dstCoord[i], vec4(dstColor[i] * energyNormalization, 0));
	}

}