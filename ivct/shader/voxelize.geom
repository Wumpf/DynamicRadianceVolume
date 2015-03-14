#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

// Input.
layout(location = 0) in vec3 vs_out_VoxelPos[];
layout(location = 1) in vec3 vs_out_WorldPos[];
layout(location = 2) in vec3 vs_out_Normal[];
layout(location = 3) in vec2 vs_out_Texcoord[];

// Ouput.
layout(location = 0) out vec3 gs_out_VoxelPos;
layout(location = 1) out vec3 gs_out_WorldPos;
layout(location = 2) out vec3 gs_out_Normal;
layout(location = 3) out vec2 gs_out_Texcoord;

#define VERTEX_PASS_THROUGH() \
	gs_out_VoxelPos = vs_out_VoxelPos[i]; \
	gs_out_WorldPos = vs_out_WorldPos[i]; \
	gs_out_Normal = vs_out_Normal[i]; \
	gs_out_Texcoord = vs_out_Texcoord[i];

void main()
{	
	vec3 v0 = vs_out_VoxelPos[1] - vs_out_VoxelPos[0];
	vec3 v1 = vs_out_VoxelPos[2] - vs_out_VoxelPos[0];  
	vec3 absNorm = abs(normalize(cross(v0,v1)));


	int i = absNorm[0] > absNorm[1] ? 0 : 1;
	i = absNorm[i] > absNorm[2] ? i : 2;

	switch(i)
	{
		// Dominant X
	case 0:
		for(int i=0; i<3; i++)
		{
			VERTEX_PASS_THROUGH();
			gl_Position = gl_in[i].gl_Position.yzxw;
			EmitVertex();
		}
		break;

		// Dominant Y	
	case 1:
		for(int i=0; i<3; i++)
		{
			VERTEX_PASS_THROUGH();
			gl_Position = gl_in[i].gl_Position.xzyw;
			EmitVertex();
		}
		break;

		// Dominant Z
	default:
		for(int i=0; i<3; i++)
		{
			VERTEX_PASS_THROUGH();
			gl_Position = gl_in[i].gl_Position.xyzw;
			EmitVertex();
		}
		break;
	}

	EndPrimitive();
}  