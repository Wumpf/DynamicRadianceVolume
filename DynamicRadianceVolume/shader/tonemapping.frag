#version 450 core

in vec2 Texcoord;

out vec3 OutputColor;

layout(location = 0) uniform float Exposure;
layout(location = 1) uniform float DragoDivider;

layout(binding=0) uniform sampler2D HDRInputTexture;

/*vec3 Reinhard(vec3 exposedColor)
{
	return exposedColor / (exposedColor + vec3(1.0));
}
vec3 ReinhardMod(vec3 exposedColor)
{
	const vec3 White = vec3(1);
	return exposedColor * (1.0 + exposedColor / White) / (exposedColor + vec3(1.0));
}*/
vec3 Drago(vec3 exposedColor)
{
	return log2(exposedColor + 1.0) / DragoDivider;//log2(LMax + 1.0);
}

void main()
{
	vec3 exposedColor = texture(HDRInputTexture, Texcoord).rgb * Exposure;

	OutputColor = Drago(exposedColor);

	// No tonemapping. Just texture output.
	//OutputColor = exposedColor;
}