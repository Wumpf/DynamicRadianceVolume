#version 450 core

#include "globalubos.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

// Input.
layout(location = 0) in vec3 vs_out_Normal[];
layout(location = 1) in vec2 vs_out_Texcoord[];

// Ouput.
layout(location = 0) out vec3 gs_out_Normal;
layout(location = 1) out vec2 gs_out_Texcoord;
layout(location = 2) out flat int gs_out_SideIndex;
layout(location = 3) out flat vec4 gs_out_RasterAABB;


void main()
{	
	vec3 v0 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 v1 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;  
	vec3 absNorm = abs(normalize(cross(v0,v1)));

	gs_out_SideIndex = absNorm[0] > absNorm[1] ? 0 : 1;
	gs_out_SideIndex = absNorm[gs_out_SideIndex] > absNorm[2] ? gs_out_SideIndex : 2;

	vec3 rasterPos[3];


	switch(gs_out_SideIndex)
	{
		// Dominant X
	case 0:
		rasterPos[0] = gl_in[0].gl_Position.zyx;
		rasterPos[1] = gl_in[1].gl_Position.zyx;
		rasterPos[2] = gl_in[2].gl_Position.zyx;
		break;

		// Dominant Y	
	case 1:
		rasterPos[0] = gl_in[0].gl_Position.xzy;
		rasterPos[1] = gl_in[1].gl_Position.xzy;
		rasterPos[2] = gl_in[2].gl_Position.xzy;
		break;

		// Dominant Z
	default:
		rasterPos[0] = gl_in[0].gl_Position.xyz;
		rasterPos[1] = gl_in[1].gl_Position.xyz;
		rasterPos[2] = gl_in[2].gl_Position.xyz;
		break;
	}


	float halfVoxelClipSpaceSize = 1.0 / VoxelResolution;

	// Compute Raster AABB
	gs_out_RasterAABB.xy = min(min(rasterPos[0].xy, rasterPos[1].xy), rasterPos[2].xy) - halfVoxelClipSpaceSize.xx;
	gs_out_RasterAABB.zw = max(max(rasterPos[0].xy, rasterPos[1].xy), rasterPos[2].xy) + halfVoxelClipSpaceSize.xx;
	gs_out_RasterAABB = (gs_out_RasterAABB * 0.5 + 0.5) * VoxelResolution;

	// Compute homogenous planes from triangle sides
	vec3 planes[3];
	vec2 a = rasterPos[0].xy - rasterPos[2].xy;
	vec2 b = rasterPos[1].xy - rasterPos[0].xy;
	planes[0] = cross(vec3(a, 0.0), vec3(rasterPos[2].xy, 1.0));
	planes[1] = cross(vec3(b, 0.0), vec3(rasterPos[0].xy, 1.0));
	planes[2] = cross(vec3(rasterPos[2].xy - rasterPos[1].xy, 0.0), vec3(rasterPos[1].xy, 1.0));

	// Negate plane normals depending on winding order
	float triangleWinding = sign(a.x * b.y - b.x * a.y);
	planes[0] *= triangleWinding;
	planes[1] *= triangleWinding;
	planes[2] *= triangleWinding;

	// Move them.
	planes[0].z -= dot(halfVoxelClipSpaceSize.xx, abs(planes[0].xy));
	planes[1].z -= dot(halfVoxelClipSpaceSize.xx, abs(planes[1].xy));
	planes[2].z -= dot(halfVoxelClipSpaceSize.xx, abs(planes[2].xy));


	#define VERTEX_PASS_THROUGH(index) \
		gl_Position.z = rasterPos[index].z; \
		gl_Position.xy /= gl_Position.w; \
		gl_Position.w = 1.0; \
		gs_out_Normal = vs_out_Normal[index]; \
		gs_out_Texcoord = vs_out_Texcoord[index]; 

	// Alternative clip/w handling:
	//gl_Position.z = gl_in[0].gl_Position.z * gl_Position.w;
	//gl_Position *= sign(gl_Position.w);
	
	// Compute new line intersections and output vertex

	gl_Position.xyw = cross(planes[0], planes[1]);
	VERTEX_PASS_THROUGH(0);
	EmitVertex();

	gl_Position.xyw = cross(planes[1], planes[2]);
	VERTEX_PASS_THROUGH(1);
	EmitVertex();

	gl_Position.xyw = cross(planes[2], planes[0]);
	VERTEX_PASS_THROUGH(2);
	EmitVertex();




	EndPrimitive();
}  