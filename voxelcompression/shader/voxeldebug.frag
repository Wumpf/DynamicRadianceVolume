#version 450 core

#include "globalubos.glsl"

// input
layout(location = 0) in vec2 Texcoord;

layout(binding = 0) uniform sampler3D VoxelScene;

// output
layout(location = 0, index = 0) out vec4 FragColor;


bool IntersectBox(vec3 rayOrigin, vec3 rayDir, vec3 aabbMin, vec3 aabbMax, out float firstHit)
{
	vec3 invRayDir = vec3(1.0) / rayDir;
	vec3 tbot = invRayDir * (aabbMin - rayOrigin);
	vec3 ttop = invRayDir * (aabbMax - rayOrigin);
	vec3 tmin = min(ttop, tbot);
	vec3 tmax = max(ttop, tbot);
	vec2 t = max(tmin.xx, tmin.yz);
	firstHit = max(0.0f, max(t.x, t.y));
	t = min(tmax.xx, tmax.yz);
	float lastHit = min(t.x, t.y);
	return firstHit <= lastHit;
}

void main()
{
	vec4 rayDir4D = vec4(Texcoord * 2.0 - vec2(1.0), 0.0f, 1.0f) * InverseViewProjection;
	vec3 rayDirection = normalize(rayDir4D.xyz / rayDir4D.w - CameraPosition);

	float rayHit = 0.0;
	if(IntersectBox(CameraPosition, rayDirection, VoxelVolumeWorldMin, VoxelVolumeWorldMax, rayHit))
		FragColor = vec4(1.0);
	else
		FragColor = vec4(abs(rayDirection), 1.0);
}
