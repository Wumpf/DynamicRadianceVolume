#version 450 core

#include "globalubos.glsl"
#include "meshdeform.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in float inBitangentHandedness;
layout(location = 4) in vec2 inTexcoord;

// Output = input for fragment shader.
out vec3 Normal;
out vec3 Tangent;
out float BitangentHandedness;
out vec2 Texcoord;

void main(void)
{
	vec3 worldPosition = (vec4(inPosition, 1.0) * World).xyz;
	gl_Position = vec4(WorldPosDeform(worldPosition), 1.0) * ViewProjection;

	// Simple pass through
	Normal = NormalDeform((vec4(inNormal, 0.0) * World).xyz, worldPosition);
	Tangent = (vec4(inTangent, 0.0) * World).xyz;
	BitangentHandedness = inBitangentHandedness;
	Texcoord = inTexcoord;
}