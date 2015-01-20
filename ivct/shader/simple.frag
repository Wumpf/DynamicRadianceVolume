#version 450 core

// Input = output from vertex shader.
in vec3 Normal;
in vec2 Texcoord;

out vec4 OutputColor;

uniform sampler2D DiffuseTexture;

void main()
{
  OutputColor = texture(DiffuseTexture, Texcoord);//vec4(abs(Normal), 1.0);     
}