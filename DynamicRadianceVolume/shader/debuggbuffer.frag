#version 450 core

in vec2 Texcoord;

out vec3 OutputColor;

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

//#define OUTPUT_POS
//#define OUTPUT_NORMAL
//#define OUTPUT_DIFFUSE
#define OUTPUT_SPECPROPS

void main()
{
#ifdef OUTPUT_DIFFUSE
	OutputColor = texture(GBuffer_Diffuse, Texcoord).rgb;
#endif

#ifdef OUTPUT_POS
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), texture(GBuffer_Depth, Texcoord).r, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;
	OutputColor = abs(worldPosition.xyz);
#endif

#ifdef OUTPUT_NORMAL
	vec3 normal = UnpackNormal16I(texture(GBuffer_Normal, Texcoord).rg);
	OutputColor = normal * 0.5 + 0.5;
	//OutputColor = vec3((vec4(normal,0) * View).z);
#endif

#ifdef OUTPUT_SPECPROPS
	OutputColor = vec3(texture(GBuffer_RoughnessMetalic, Texcoord).rg, 0.0).rbg;
#endif
}