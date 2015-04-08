#version 450 core

#include "utils.glsl"

layout(early_fragment_tests) in; // Force early z.

layout(location = 0) in vec3 inNormal;
layout(location = 0) in vec2 inTexcoord;

layout(location = 0) out vec4 OutData;

uniform sampler2D DiffuseTexture;

void main()
{
	OutData.xy = PackNormal(normalize(inNormal));
	OutData.z = gl_FragCoord.z;
	OutData.w = inTexcoord.x; // todo - fill in roughness
}