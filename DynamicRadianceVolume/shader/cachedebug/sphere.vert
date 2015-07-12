#version 450 core

#include "../globalubos.glsl"

#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "../lightcache.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in float inBitangentHandedness;
layout(location = 4) in vec2 inTexcoord;

// Output = input for fragment shader.
out VertexOutput
{
	vec3 LocalPosition;
	vec3 WorldPosition;
	flat int InstanceID;
} Out;

void main(void)
{
	vec3 worldPosition = inPosition * 0.1 + LightCacheEntries[gl_InstanceID].Position;
	gl_Position = vec4(worldPosition, 1.0) * ViewProjection;

	Out.LocalPosition = inPosition;
	Out.WorldPosition = worldPosition;
	Out.InstanceID = gl_InstanceID;
}