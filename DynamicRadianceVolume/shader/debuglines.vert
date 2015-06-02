#version 450 core

#include "globalubos.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;

void main(void)
{
	gl_Position = vec4(inPosition, 1.0) * ViewProjection;
}