#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

// Input.
layout(location = 0) in vec3 vs_out_VoxelPos[];

// Ouput.
layout(location = 0) out vec3 gs_out_VoxelPos;

void main()
{	
	vec3 v0 = vs_out_VoxelPos[1] - vs_out_VoxelPos[0];
	vec3 v1 = vs_out_VoxelPos[2] - vs_out_VoxelPos[0];  

	float areaXY = abs(v0.x * v1.y - v1.x * v0.y);
	float areaYZ = abs(v0.y * v1.z - v1.y * v0.z);
	float areaZX = abs(v0.x * v1.z - v1.x * v0.z);

	if(areaXY > areaYZ)
	{
		if(areaXY > areaZX)
		{
			for(int i=0; i<3; i++)
			{
				gs_out_VoxelPos = vs_out_VoxelPos[i];
				gl_Position = gl_in[i].gl_Position;
				EmitVertex();
			}
		}
		else
		{
			for(int i=0; i<3; i++)
			{
				gs_out_VoxelPos = vs_out_VoxelPos[i];
				gl_Position = gl_in[i].gl_Position.zxyw;
				EmitVertex();
			}
		}
	}
	else
	{
		if(areaYZ > areaZX)
		{
			for(int i=0; i<3; i++)
			{
				gs_out_VoxelPos = vs_out_VoxelPos[i];
				gl_Position = gl_in[i].gl_Position.yzxw;
				EmitVertex();
			}
		}
		else
		{
			for(int i=0; i<3; i++)
			{
				gs_out_VoxelPos = vs_out_VoxelPos[i];
				gl_Position = gl_in[i].gl_Position.zxyw;
				EmitVertex();
			}
		}
	}
	EndPrimitive();
}  