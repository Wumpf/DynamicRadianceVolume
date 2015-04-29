#version 450 core
#extension GL_NV_shader_atomic_float : require

#include "globalubos.glsl"
#include "lightcache.glsl"
#include "gbuffer.glsl"
#include "utils.glsl"

layout(location = 0) in vec3 gs_out_Normal;
layout(location = 1) in vec2 gs_out_Texcoord;
layout(location = 2) in flat int gs_out_SideIndex;
layout(location = 3) in flat vec4 gs_out_RasterAABB;

layout(binding = 0, r8) restrict writeonly uniform 	image3D VoxelVolume;

ivec3 UnswizzlePos(ivec3 pos)
{
	return gs_out_SideIndex == 0 ? pos.zyx : (gs_out_SideIndex == 1 ? pos.xzy : pos.xyz);
}

void main()
{
	// Clip pixels outside of conservative raster AABB
	if(any(lessThan(gl_FragCoord.xy, gs_out_RasterAABB.xy)) || any(greaterThan(gl_FragCoord.xy, gs_out_RasterAABB.zw)))
	{
		return; // Same as discard here.
	}
	
	// Retrieve voxel pos from gl_FragCoord
	// Attention: This voxel pos is still swizzled!
	vec3 voxelPosSwizzled;
	voxelPosSwizzled.xy = gl_FragCoord.xy;
	voxelPosSwizzled.z = gl_FragCoord.z*VoxelResolution;
	ivec3 voxelPosSwizzledI = ivec3(voxelPosSwizzled);

	imageStore(VoxelVolume, UnswizzlePos(voxelPosSwizzledI), vec4(1));

	// "Depth Conservative"
	float depthDx = dFdxCoarse(voxelPosSwizzled.z);
	float depthDy = dFdyCoarse(voxelPosSwizzled.z);
	float maxChange = length(vec2(depthDx, depthDy)) * 1.414; // * inversesqrt(2);

	float minDepth = voxelPosSwizzled.z - maxChange;
	float maxDepth = voxelPosSwizzled.z + maxChange;

	if(voxelPosSwizzledI.z != int(minDepth))
	{
		ivec3 voxelPosMin = voxelPosSwizzledI;
		voxelPosMin.z -= 1;
		imageStore(VoxelVolume, UnswizzlePos(voxelPosMin), vec4(1));
	}
	if(voxelPosSwizzledI.z != int(maxDepth))
  	{
  		ivec3 voxelPosMax = voxelPosSwizzledI;
		voxelPosMax.z += 1;
  		imageStore(VoxelVolume, UnswizzlePos(voxelPosMax), vec4(1));
  	}
}