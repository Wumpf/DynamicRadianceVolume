#version 450 core

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"

#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "lightcache.glsl"

in vec2 Texcoord;

out vec3 OutputColor;

vec3 Interp(vec3 x)
{
	return x;
	//return x*x*(3.0 - 2.0*x);
}

void main()
{
	// Get pixel world position.
	float depthBufferDepth = textureLod(GBuffer_Depth, Texcoord, 0.0).r;
	if(depthBufferDepth < 0.00001)
	{
		discard;
	}
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

	// Get normal and material color
	vec3 worldNormal = UnpackNormal16I(textureLod(GBuffer_Normal, Texcoord, 0.0).rg);
	vec3 diffuse = texture(GBuffer_Diffuse, Texcoord).rgb;

	vec3 addressCoord = (worldPosition - VolumeWorldMin) / AddressVolumeVoxelSize;
	ivec3 addressCoord00 = ivec3(addressCoord);

	ivec3 offsets[8] =
	{
		ivec3(0,0,0),
		ivec3(1,0,0),
		ivec3(0,1,0),
		ivec3(1,1,0),

		ivec3(0,0,1),
		ivec3(1,0,1),
		ivec3(0,1,1),
		ivec3(1,1,1)
	};

	vec3 irradiance[8];

	for(int i=0; i<8; ++i)
	{
		uint cacheAddress = texelFetch(VoxelAddressVolume, addressCoord00 + offsets[i], 0).r;

		// Check if address is valid. (debug code!)
		/*if(cacheAddress == 0 || cacheAddress == 0xFFFFFFFF)
		{
			OutputColor = vec3(1,0,1);
			return;
		}*/
		cacheAddress -= 1;

		// -----------------------------------------------
		// IRRADIANCE VIA SH (test)

		const float factor0 = 1.0 / (2.0 * sqrt(PI));
		const float factor1 = sqrt(3.0) / (2.0 * sqrt(PI));

		// Band 0
		irradiance[i] = vec3(LightCacheEntries[cacheAddress].SH00_r,
								LightCacheEntries[cacheAddress].SH00_g,
								LightCacheEntries[cacheAddress].SH00_b) * factor0;

		// Band 1
		irradiance[i] += LightCacheEntries[cacheAddress].SH1neg1 * (factor1 * worldNormal.y);
		irradiance[i] += LightCacheEntries[cacheAddress].SH10 * (factor1 * worldNormal.z);
		irradiance[i] += LightCacheEntries[cacheAddress].SH1pos1 * (factor1 * worldNormal.x);

		// Band 2
	/*	irradiance[i] += LightCacheEntries[cacheAddress].SH2neg2 * (sqrt(15.0 / (4.0 * PI)) * worldNormal.x * worldNormal.y);
		irradiance[i] += LightCacheEntries[cacheAddress].SH2neg1 * (sqrt(15.0 / (4.0 * PI)) * worldNormal.y * worldNormal.z);
		irradiance[i] += vec3(LightCacheEntries[cacheAddress].SH20_r,
								LightCacheEntries[cacheAddress].SH20_g,
								LightCacheEntries[cacheAddress].SH20_b) * (sqrt(5.0 / (16.0 * PI)) * (worldNormal.z * worldNormal.z * 3.0 - 1.0));
		irradiance[i] += LightCacheEntries[cacheAddress].SH2pos1 * (sqrt(15.0 / (8.0 * PI))* worldNormal.x * worldNormal.z);
		irradiance[i] += LightCacheEntries[cacheAddress].SH2pos2 * (sqrt(15.0 / (32.0 * PI)) * (worldNormal.x * worldNormal.x - worldNormal.y * worldNormal.y));	
		*/
	}

	// trilinear interpolation
	vec3 interp = Interp(addressCoord - addressCoord00);
	vec3 interpolatedIrradiance = 
		mix(mix(mix(irradiance[0], irradiance[1], interp.x),
				mix(irradiance[2], irradiance[3], interp.x), interp.y),
			mix(mix(irradiance[4], irradiance[5], interp.x),
				mix(irradiance[6], irradiance[7], interp.x), interp.y), interp.z);


	OutputColor = interpolatedIrradiance * diffuse;
	//OutputColor = mod(addressCoord00, vec3(64)) / 64.0;
	//OutputColor = worldNormal * 0.5 + vec3(0.5);
} 