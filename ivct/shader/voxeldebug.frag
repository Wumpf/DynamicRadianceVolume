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
	vec4 rayDir4D = vec4(Texcoord * 2.0 - vec2(1.0), 1.0f, 1.0f) * InverseViewProjection;
	vec3 rayDirection = normalize(rayDir4D.xyz / rayDir4D.w - CameraPosition);

	FragColor = vec4(abs(rayDirection), 1.0);

	float rayHit = 0.0;
	if(IntersectBox(CameraPosition, rayDirection, VoxelVolumeWorldMin, VoxelVolumeWorldMax, rayHit))
	{
		float stepSize = VoxelSizeInWorld.x * 0.1;

		vec3 voxelHitPos = (CameraPosition + (rayHit + stepSize) * rayDirection - VoxelVolumeWorldMin) / VoxelSizeInWorld;
		float totalIntensity = 0.0f;

		vec3 rayMarchStep = rayDirection * stepSize / VoxelSizeInWorld;

		// Simple raycast
		vec3 voxelVolumeSize = vec3(textureSize(VoxelScene, 0));
		while(all(greaterThanEqual(voxelHitPos, vec3(0))) && 
			  all(lessThan(voxelHitPos, voxelVolumeSize)))
		{
			// Check current voxel.
			ivec3 voxelCoord = ivec3(voxelHitPos + vec3(0.5));
			totalIntensity += texelFetch(VoxelScene, voxelCoord, 0).r;	
			if(totalIntensity >= 1.0)
			{
				// mark edges red.
				/*vec3 edgeDist = clamp(abs(voxelCoord - voxelHitPos) * 2.0, vec3(0.0), vec3(1.0));
				edgeDist = pow(edgeDist, vec3(8.0));
				float edgeFactor = max(max(edgeDist.x*edgeDist.y, edgeDist.x * edgeDist.z), edgeDist.y * edgeDist.z);
				FragColor = vec4(mix(vec3(0.0), vec3(1.0, 0.2, 0.2), edgeFactor), 1.0);*/

				FragColor = vec4(mod(voxelCoord/32.0, vec3(1.0)), 1.0);

				break;
			}

			voxelHitPos += rayMarchStep * stepSize;
			rayMarchStep *= 1.000001;
		}

		//FragColor = vec4(mix(FragColor.xyz, vec3(1.0), totalIntensity), 0);
	}
}
