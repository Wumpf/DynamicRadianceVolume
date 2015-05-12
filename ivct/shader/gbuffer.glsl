layout(binding=0) uniform sampler2D GBuffer_Diffuse;
layout(binding=1) uniform sampler2D GBuffer_Specular; // RGB specular, A roughness
layout(binding=2) uniform isampler2D GBuffer_Normal;
layout(binding=3) uniform sampler2D GBuffer_Depth;