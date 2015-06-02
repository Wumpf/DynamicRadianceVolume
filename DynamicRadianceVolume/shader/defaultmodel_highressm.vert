#version 450 core

#include "globalubos.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in float inBitangentHandedness;
layout(location = 4) in vec2 inTexcoord;

// Output = input for fragment shader.
out vec3 Position;

#ifdef ALPHATESTING
out vec2 Texcoord;
#endif

void main(void)
{
	Position = (vec4(inPosition, 1.0) * World).xyz;
	gl_Position = vec4(Position, 1.0) * LightViewProjection;
#ifdef ALPHATESTING
	Texcoord = inTexcoord;
#endif
}