#version 450 core

layout(location = 0) in vec3 gs_out_VoxelPos;
layout(location = 4) in flat int gs_out_SideIndex;

layout(binding = 0, r8) restrict writeonly uniform image3D VoxelVolume;

//layout(location = 0, index = 0) out vec4 FragColor;

void main()
{
	ivec3 voxelVolumeSize = imageSize(VoxelVolume);
	ivec3 voxelPos = ivec3(gs_out_VoxelPos * voxelVolumeSize + vec3(0.5));
	imageStore(VoxelVolume, voxelPos, vec4(1.0));

	// "Depth Conservative"
	float depthDx = dFdxCoarse(gs_out_VoxelPos[gs_out_SideIndex]);
	float depthDy = dFdyCoarse(gs_out_VoxelPos[gs_out_SideIndex]);
	float maxChange = length(vec2(depthDx, depthDy)) * inversesqrt(2);

	float minDepth = gs_out_VoxelPos[gs_out_SideIndex] - maxChange;
	float maxDepth = gs_out_VoxelPos[gs_out_SideIndex] + maxChange;

	int minDepthVoxel = int(minDepth * voxelVolumeSize[gs_out_SideIndex] + vec3(0.5));
	int maxDepthVoxel = int(maxDepth * voxelVolumeSize[gs_out_SideIndex] + vec3(0.5));

	if(voxelPos[gs_out_SideIndex] != minDepthVoxel)
	{
		ivec3 voxelPosMin = voxelPos;
		voxelPosMin[gs_out_SideIndex] -= 1;
		imageStore(VoxelVolume, voxelPosMin, vec4(1.0));
	}
	else if(voxelPos[gs_out_SideIndex] != maxDepthVoxel) // Else by convention! Perfect diagonal case might affect 3 voxel
  	{
  		ivec3 voxelPosMax = voxelPos;
		voxelPosMax[gs_out_SideIndex] += 1;
  		imageStore(VoxelVolume, voxelPosMax, vec4(1.0));
  	}

  //FragColor = vec4(1.0);
}