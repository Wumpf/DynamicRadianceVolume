#version 450 core

in vec3 VoxelPos;

layout(binding = 0, r8) restrict writeonly uniform image3D VoxelVolume;

//layout(location = 0, index = 0) out vec4 FragColor;

void main()
{
  ivec3 voxelVolumeSize = imageSize(VoxelVolume);
  imageStore(VoxelVolume, ivec3(VoxelPos * voxelVolumeSize + vec3(0.5)), vec4(1.0));

  //FragColor = vec4(1.0);
}