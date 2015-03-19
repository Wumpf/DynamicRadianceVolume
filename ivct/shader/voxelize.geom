#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

// Input.
layout(location = 0) in vec3 vs_out_Normal[];
layout(location = 1) in vec2 vs_out_Texcoord[];

// Ouput.
layout(location = 0) out vec3 gs_out_Normal;
layout(location = 1) out vec2 gs_out_Texcoord;
layout(location = 2) out flat int gs_out_SideIndex;

#define VERTEX_PASS_THROUGH() \
	gs_out_Normal = vs_out_Normal[i]; \
	gs_out_Texcoord = vs_out_Texcoord[i]; \
	gs_out_SideIndex = sideIndex;

void main()
{	
	vec3 v0 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 v1 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;  
	vec3 absNorm = abs(normalize(cross(v0,v1)));


	int sideIndex = absNorm[0] > absNorm[1] ? 0 : 1;
	sideIndex = absNorm[sideIndex] > absNorm[2] ? sideIndex : 2;

	switch(sideIndex)
	{
		// Dominant X
	case 0:
		for(int i=0; i<3; i++)
		{
			VERTEX_PASS_THROUGH();
			gl_Position = gl_in[i].gl_Position.zyxw;
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