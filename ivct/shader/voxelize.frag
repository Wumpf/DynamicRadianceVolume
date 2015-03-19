#version 450 core

layout(location = 2) in flat int gs_out_SideIndex;

layout(binding = 0, r8) restrict writeonly uniform image3D VoxelVolume;

//layout(location = 0, index = 0) out vec4 FragColor;

ivec3 UnswizzlePos(ivec3 pos)
{
	return gs_out_SideIndex == 0 ? pos.zyx : (gs_out_SideIndex == 1 ? pos.xzy : pos.xyz);
}

void main()
{
	// Retrieve voxel pos from gl_FragCoord
	// Attention: This voxel pos is still swizzled!
	int voxelVolumeSize = imageSize(VoxelVolume).x;
	vec3 voxelPos;
	voxelPos.xy = gl_FragCoord.xy;
	voxelPos.z = gl_FragCoord.z*voxelVolumeSize;
	ivec3 voxelPosI = ivec3(voxelPos + vec3(0.5));

	imageStore(VoxelVolume, UnswizzlePos(voxelPosI), vec4(1.0));

	// "Depth Conservative"
	float depthDx = dFdxCoarse(voxelPos.z);
	float depthDy = dFdyCoarse(voxelPos.z);
	float maxChange = length(vec2(depthDx, depthDy)) * inversesqrt(2);

	float minDepth = voxelPos.z - maxChange;
	float maxDepth = voxelPos.z + maxChange;

	int minDepthVoxel = int(minDepth * voxelVolumeSize + vec3(0.5));
	int maxDepthVoxel = int(maxDepth * voxelVolumeSize + vec3(0.5));

	if(voxelPosI.z != minDepthVoxel)
	{
		ivec3 voxelPosMin = voxelPosI;
		voxelPosMin -= 1;
		imageStore(VoxelVolume, UnswizzlePos(voxelPosMin), vec4(1.0));
	}
	else if(voxelPosI.z != maxDepthVoxel) // Else by convention! Perfect diagonal case might affect 3 voxel
  	{
  		ivec3 voxelPosMax = voxelPosI;
		voxelPosMax.z += 1;
  		imageStore(VoxelVolume, UnswizzlePos(voxelPosMax), vec4(1.0));
  	}

  //FragColor = vec4(1.0);
}