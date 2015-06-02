#version 450 core

// Options:
//#define ALPHATESTING <DesiredAlphaThreshhold>

#ifdef ALPHATESTING
layout(binding = 0) uniform sampler2D BaseColorTexture;
in vec2 Texcoord;
#endif

void main()
{
#ifdef ALPHATESTING
	if(texture(BaseColorTexture, Texcoord).a < 0.1)
		discard;
#endif
}