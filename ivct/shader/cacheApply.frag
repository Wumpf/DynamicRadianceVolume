#version 450 core

#define CONTACT_SHADOW_FIX

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "lightcache.glsl"

layout(binding=4) uniform usampler3D VoxelAddressVolume;

#ifdef CONTACT_SHADOW_FIX
layout(binding=5) uniform sampler3D VoxelVolume;
#endif

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

	// Get normal and material colors
	vec3 worldNormal = UnpackNormal16I(textureLod(GBuffer_Normal, Texcoord, 0.0).rg);
	vec3 baseColor = texture(GBuffer_Diffuse, Texcoord).rgb;
	vec2 roughnessMetalic = texture(GBuffer_RoughnessMetalic, Texcoord).rg;
	float blinnExponent = RoughnessToBlinnExponent(roughnessMetalic.x);
	vec3 diffuseColor, specularColor;
	ComputeMaterialColors(baseColor, roughnessMetalic.y, diffuseColor, specularColor);


	//vec3 addressCoord = (worldPosition - VolumeWorldMin) / AddressVolumeVoxelSize;
	vec3 addressCoord = clamp((worldPosition - VolumeWorldMin) / AddressVolumeVoxelSize, vec3(0), vec3(AddressVolumeResolution-0.999));
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


	{
		vec3 irradiance[8];

		#if defined(INDDIFFUSE_VIA_SH1) || defined(INDDIFFUSE_VIA_SH2)
			const float shEvaFactor0 = 1.0 / (2.0 * sqrt(PI));
			const float shEvaFactor1 = sqrt(3.0) / (2.0 * sqrt(PI));

			const float shEvaFactor2n2 = sqrt(15.0 / (4.0 * PI));
			const float shEvaFactor2n1 = -sqrt(15.0 / (4.0 * PI));
			const float shEvaFactor20 = sqrt(5.0 / (16.0 * PI));
			const float shEvaFactor2p1 =  -sqrt(15.0 / (4.0 * PI));
			const float shEvaFactor2p2 =  sqrt(15.0 / (16.0 * PI));

		#elif defined(INDDIFFUSE_VIA_H)
			const float factor0 = 1.0 / (2.0 * PI);
			const float factor1 = sqrt(3.0) / sqrt(2.0 * PI);
			const float factor2 = sqrt(15.0) / sqrt(2.0 * PI);
		#endif



		for(int i=0; i<8; ++i)
		{
			ivec3 cacheSamplePos = addressCoord00 + offsets[i];
			uint cacheAddress = texelFetch(VoxelAddressVolume, cacheSamplePos, 0).r;

			// Check if address is valid. (debug code!)
			/*if(cacheAddress == 0 || cacheAddress == 0xFFFFFFFF)
			{
				OutputColor = vec3(1,0,1);
				return;
			}*/
			cacheAddress -= 1;

			#if defined(INDDIFFUSE_VIA_H)
				vec3 cacheWorldPosition = cacheSamplePos * AddressVolumeVoxelSize + VolumeWorldMin;
				vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(cacheWorldPosition);
			#endif

			// -----------------------------------------------
			// IRRADIANCE VIA SH
		#if defined(INDDIFFUSE_VIA_SH1) || defined(INDDIFFUSE_VIA_SH2)
			// Band 0
			irradiance[i] = vec3(LightCacheEntries[cacheAddress].SH00_r,
									LightCacheEntries[cacheAddress].SH00_g,
									LightCacheEntries[cacheAddress].SH00_b) * shEvaFactor0;

			// Band 1
			irradiance[i] -= LightCacheEntries[cacheAddress].SH1neg1 * (shEvaFactor1 * worldNormal.y);
			irradiance[i] += LightCacheEntries[cacheAddress].SH10 * (shEvaFactor1 * worldNormal.z);
			irradiance[i] -= LightCacheEntries[cacheAddress].SH1pos1 * (shEvaFactor1 * worldNormal.x);

			// Band 2
			#ifdef INDDIFFUSE_VIA_SH2
			irradiance[i] += LightCacheEntries[cacheAddress].SH2neg2 * (shEvaFactor2n2 * worldNormal.x * worldNormal.y);
			irradiance[i] += LightCacheEntries[cacheAddress].SH2neg1 * (shEvaFactor2n1 * worldNormal.y * worldNormal.z);
			irradiance[i] += vec3(LightCacheEntries[cacheAddress].SH20_r,
									LightCacheEntries[cacheAddress].SH20_g,
									LightCacheEntries[cacheAddress].SH20_b) * (shEvaFactor20 * (worldNormal.z * worldNormal.z * 3.0 - 1.0));
			irradiance[i] += LightCacheEntries[cacheAddress].SH2pos1 * (shEvaFactor2p1 * worldNormal.x * worldNormal.z);
			irradiance[i] += LightCacheEntries[cacheAddress].SH2pos2 * (shEvaFactor2p2 * (worldNormal.x * worldNormal.x - worldNormal.y * worldNormal.y));	

			#endif

			// -----------------------------------------------
			// IRRADIANCE VIA H-Basis
		#elif defined(INDDIFFUSE_VIA_H)
			irradiance[i]  = LightCacheEntries[cacheAddress].irradianceH1 * factor0;
			irradiance[i] -= LightCacheEntries[cacheAddress].irradianceH2 * factor1 * cacheViewNormal.y;
			irradiance[i] += LightCacheEntries[cacheAddress].irradianceH3 * factor1 * (2.0 * cacheViewNormal.z - 1.0);
			irradiance[i] -= vec3(LightCacheEntries[cacheAddress].irradianceH4r,
									LightCacheEntries[cacheAddress].irradianceH4g,
									LightCacheEntries[cacheAddress].irradianceH4b) * (factor1 * cacheViewNormal.x);
			#if INDDIFFUSE_VIA_H > 4
			irradiance[i] += LightCacheEntries[cacheAddress].irradianceH5 * (factor2 * cacheViewNormal.x * cacheViewNormal.y);
			irradiance[i] += LightCacheEntries[cacheAddress].irradianceH6 * (factor2 * (cacheViewNormal.x * cacheViewNormal.x - cacheViewNormal.y * cacheViewNormal.y) * 0.5);
			#endif
		#endif


			// Negative irradiance values are not meaningful (may happen due to SH overshooting)
			irradiance[i] = max(irradiance[i], vec3(0.0));
		}

		// trilinear interpolation
		vec3 interp = Interp(addressCoord - addressCoord00);
		vec3 interpolatedIrradiance = 
			mix(mix(mix(irradiance[0], irradiance[1], interp.x),
					mix(irradiance[2], irradiance[3], interp.x), interp.y),
				mix(mix(irradiance[4], irradiance[5], interp.x),
					mix(irradiance[6], irradiance[7], interp.x), interp.y), interp.z);

		OutputColor = interpolatedIrradiance * diffuseColor / PI;
	}

	// Test code for "Cache Local View Space"
	/*{
		vec3 cacheWorldPosition = addressCoord00 * AddressVolumeVoxelSize + VolumeWorldMin;
		vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(cacheWorldPosition);
		OutputColor = vec3(cacheViewNormal.z < 0 ? 1.0 : -1.0); // Check for negative normals
		OutputColor = cacheViewNormal * 0.5 + 0.5; // Display
	}*/

	//uint cacheAddress = texelFetch(VoxelAddressVolume, addressCoord00, 0).r;
	//OutputColor = LightCacheEntries[cacheAddress+1].AverageNormal * 0.5 + 0.5; // Display
} 