#version 450 core

#include "utils.glsl"

in vec2 Texcoord;

layout(location = 0) out vec3 OutFlux;
layout(location = 1) out ivec2 OutPackedNormal;
layout(location = 2) out vec2 OutDepthLinSq;

layout(binding = 0) uniform sampler2D Flux;
layout(binding = 1) uniform isampler2D PackedNormal;
layout(binding = 2) uniform sampler2D DepthLinSq;

void main()
{
	vec4 gatherFlux = textureGather(Flux, Texcoord, 0);
	OutFlux.r = gatherFlux.r + gatherFlux.g + gatherFlux.b + gatherFlux.a;
	gatherFlux = textureGather(Flux, Texcoord, 1);
	OutFlux.g = gatherFlux.r + gatherFlux.g + gatherFlux.b + gatherFlux.a;
	gatherFlux = textureGather(Flux, Texcoord, 2);
	OutFlux.b = gatherFlux.r + gatherFlux.g + gatherFlux.b + gatherFlux.a;

	ivec4 normalTheta = textureGather(PackedNormal, Texcoord, 0);
	ivec4 normalPhi = textureGather(PackedNormal, Texcoord, 1);
	vec3 normal = UnpackNormal16I(ivec2(normalTheta.x, normalPhi.x)) + 
					UnpackNormal16I(ivec2(normalTheta.y, normalPhi.y)) +
					UnpackNormal16I(ivec2(normalTheta.z, normalPhi.z)) +
					UnpackNormal16I(ivec2(normalTheta.w, normalPhi.w));
	OutPackedNormal = PackNormal16I(normalize(normal));

	OutDepthLinSq = textureLod(DepthLinSq, Texcoord, 0).rg;
}