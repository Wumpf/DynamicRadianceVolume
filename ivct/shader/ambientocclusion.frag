#version 450 core

// See https://research.nvidia.com/sites/default/files/publications/GIVoxels-pg2011-authors.pdf

layout(binding=3) uniform sampler3D VoxelVolume;

in vec2 Texcoord;

out vec3 OutputColor;

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

// Create ONB from normalized vector
void CreateONB(in vec3 n, out vec3 U, out vec3 V)
{
	U = cross(n, vec3(0.0, 1.0, 0.0));
	if (all(lessThan(abs(U), vec3(0.0001))))
		U = cross(n, vec3(1.0, 0.0, 0.0));
	U = normalize(U);
	V = cross(n, U);
}

void main()
{
	// Read G-Buffer pos & normal
	float depthBufferDepth = texture(GBuffer_Depth, Texcoord).r;
	if(depthBufferDepth < 0.000001)
		discard;
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;
	vec3 worldNormal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);

	// Todo: Experience says that constants in glsl are super slow, should use UBO instead.
	const int NumDirs = 6;
	const vec4 SampleDirsWeight[NumDirs] = // Weight = cos(theta) integrated over cone area
	{
		vec4(0.0, 1.0, 0.0, PI / 4.0),
		vec4(0.0, 0.5, 0.866025, 3.0 * PI / 20.0),
		vec4(0.823639, 0.5, 0.267617, 3.0 * PI / 20.0),
		vec4(0.509037, 0.5, -0.700629, 3.0 * PI / 20.0),
		vec4(-0.509037, 0.5, -0.700629, 3.0 * PI / 20.0),
		vec4(-0.823639, 0.5, 0.267617, 3.0 * PI / 20.0),
	};
	const float directionRadius = PI/3.0;
	const float distToSphereRad = sin(directionRadius * 0.5);


	vec3 U, V;
	CreateONB(worldNormal, U, V);

	ivec3 volumeSize = textureSize(VoxelVolume, 0);

	vec3 startPositionWorld = worldPosition + worldNormal * VoxelSizeInWorld * 1.6; //sqrt(2.0) * someFudge;
	vec3 startPositionVoxel = (startPositionWorld - VolumeWorldMin) / (VoxelSizeInWorld * volumeSize);

	float totalOcclusion = 0.0;

	for(int d=0; d<NumDirs; ++d)
	{
		vec3 dirInWorld = SampleDirsWeight[d].x * V + SampleDirsWeight[d].y * worldNormal + SampleDirsWeight[d].z * U;
		vec3 dirInVoxel = dirInWorld / volumeSize;

		vec3 currentPosVoxel = startPositionVoxel;
		float stepSize = 1.0; // Start with stepping one voxel
		float dist = 0.0;
		float coneWeight = 0.0;

		// Run for a maximum step size as long as coneWeight is smaller than weight and we are not outside of the volume
		const int maxNumSteps = 16;
		for(int s=0; s<maxNumSteps && coneWeight < 0.99 && saturate(currentPosVoxel) == currentPosVoxel; ++s)
		{
			currentPosVoxel += dirInVoxel * stepSize;
			dist += stepSize;

			float currentSphereRadius = dist * distToSphereRad;

			// Lookup in miplevel that has voxels of the size of the current sphere radius.
			float newOcclusion = textureLod(VoxelVolume, currentPosVoxel, log2(currentSphereRadius)).r; 
			coneWeight += (1.0 - coneWeight) * newOcclusion;

			stepSize = currentSphereRadius * 2.0;
		}
		totalOcclusion += coneWeight * SampleDirsWeight[d].w / NumDirs;
	}

	OutputColor = vec3(saturate(1.0 - totalOcclusion));
}