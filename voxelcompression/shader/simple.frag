#version 450 core

// Input = output from vertex shader.
in vec3 Normal;
in vec2 Texcoord;

out vec4 OutputColor;

void main()
{
  OutputColor = vec4(abs(Normal), 1.0);     
}