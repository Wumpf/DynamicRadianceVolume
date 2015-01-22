#version 450 core

#include "globalubos.glsl"

// Vertex input.
layout(location = 0) in vec3 inPosition;

// Output
out vec3 VoxelPos; // Ranging from 0 to 1

void main(void)
{
  VoxelPos = (inPosition - VoxelVolumeWorldMin) /
              (VoxelVolumeWorldMax - VoxelVolumeWorldMin);
  gl_Position = vec4(VoxelPos * 2.0 - vec3(1.0), 1.0);
}