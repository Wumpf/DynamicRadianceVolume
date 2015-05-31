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

layout(binding=6) uniform sampler2D CacheSpecularEnvmap;

in vec2 Texcoord;

out vec3 OutputColor;

vec3 Interp(vec3 x)
{
	return x;
	//return x*x*(3.0 - 2.0*x);
}

void main()
{	
	//OutputColor = texelFetch(CacheSpecularEnvmap, ivec2(Texcoord * BackbufferResolution/4), 0).rgb; // TODO
	//return;

	// Get pixel world position.
	float depthBufferDepth = textureLod(GBuffer_Depth, Texcoord, 0.0).r;
	if(depthBufferDepth < 0.00001)
	{
		discard;
	}
	vec4 worldPosition4D = vec4(Texcoord * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

	// Get normal.
	vec3 worldNormal = UnpackNormal16I(textureLod(GBuffer_Normal, Texcoord, 0.0).rg);

	// Sample material data
	vec3 baseColor = texture(GBuffer_Diffuse, Texcoord).rgb;
	vec2 roughnessMetalic = texture(GBuffer_RoughnessMetalic, Texcoord).rg;
	vec3 diffuseColor, specularColor;
	ComputeMaterialColors(baseColor, roughnessMetalic.y, diffuseColor, specularColor);

#ifdef INDIRECT_SPECULAR
	float blinnExponent = RoughnessToBlinnExponent(roughnessMetalic.x);
	float specularEnvmapLod = GetHemisphereLodForBlinnPhongExponent(blinnExponent, SpecularEnvmapPerCacheSize_Texel);
	float maxHalfSpecularEnvmapPixelSize = 0.5 / (pow(2.0, -ceil(specularEnvmapLod)) * SpecularEnvmapPerCacheSize_Texel);
	//OutputColor = vec3(specularEnvmapLod) * 0.25;
	//return;
#endif

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
		vec3 specular[8];

		#ifdef INDDIFFUSE_VIA_H
			const float factor0 = 1.0 / (2.0 * PI);
			const float factor1 = sqrt(3.0) / sqrt(2.0 * PI);
			const float factor2 = sqrt(15.0) / sqrt(2.0 * PI);
		#endif

		vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(worldPosition);

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

			/*
			#if defined(INDDIFFUSE_VIA_H) || defined(INDIRECT_SPECULAR)
				vec3 cacheWorldPosition = cacheSamplePos * AddressVolumeVoxelSize + VolumeWorldMin;
				vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(cacheWorldPosition);

				//cacheViewNormal.z = saturate(cacheViewNormal.z);
				//cacheViewNormal = normalize(cacheViewNormal);
			#endif
			*/

		#ifdef INDIRECT_SPECULAR
			vec2 cacheSpecularEnvmapOffset = vec2(cacheAddress % SpecularEnvmapNumCachesPerDimension, cacheAddress / SpecularEnvmapNumCachesPerDimension);
			vec2 hemiProjection = HemisphericalProjection(cacheViewNormal);
			hemiProjection = clamp(hemiProjection, vec2(maxHalfSpecularEnvmapPixelSize), vec2(1.0 - maxHalfSpecularEnvmapPixelSize)); // Make sure not to filter pixels from neighboring caches.
			vec2 cacheSpecularEnvmapTex = (hemiProjection + cacheSpecularEnvmapOffset) * SpecularEnvmapPerCacheSize_Texcoord;
			specular[i] = textureLod(CacheSpecularEnvmap, cacheSpecularEnvmapTex, specularEnvmapLod).rgb;
		#endif

			// -----------------------------------------------
			// IRRADIANCE VIA SH
		#if defined(INDDIFFUSE_VIA_SH1) || defined(INDDIFFUSE_VIA_SH2)
			// Band 0
			irradiance[i] = vec3(LightCacheEntries[cacheAddress].SH00_r,
									LightCacheEntries[cacheAddress].SH00_g,
									LightCacheEntries[cacheAddress].SH00_b) * ShEvaFactor0;

			// Band 1
			irradiance[i] -= LightCacheEntries[cacheAddress].SH1neg1 * (ShEvaFactor1 * worldNormal.y);
			irradiance[i] += LightCacheEntries[cacheAddress].SH10 * (ShEvaFactor1 * worldNormal.z);
			irradiance[i] -= LightCacheEntries[cacheAddress].SH1pos1 * (ShEvaFactor1 * worldNormal.x);

			// Band 2
			#ifdef INDDIFFUSE_VIA_SH2
			irradiance[i] += LightCacheEntries[cacheAddress].SH2neg2 * (ShEvaFactor2n2 * worldNormal.x * worldNormal.y);
			irradiance[i] += LightCacheEntries[cacheAddress].SH2neg1 * (ShEvaFactor2n1 * worldNormal.y * worldNormal.z);
			irradiance[i] += vec3(LightCacheEntries[cacheAddress].SH20_r,
									LightCacheEntries[cacheAddress].SH20_g,
									LightCacheEntries[cacheAddress].SH20_b) * (ShEvaFactor20 * (worldNormal.z * worldNormal.z * 3.0 - 1.0));
			irradiance[i] += LightCacheEntries[cacheAddress].SH2pos1 * (ShEvaFactor2p1 * worldNormal.x * worldNormal.z);
			irradiance[i] += LightCacheEntries[cacheAddress].SH2pos2 * (ShEvaFactor2p2 * (worldNormal.x * worldNormal.x - worldNormal.y * worldNormal.y));	

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
		// TODO: Interpolate earlier to lower register pressure!
		vec3 interp = Interp(addressCoord - addressCoord00);
		vec3 interpolatedIrradiance = 
			mix(mix(mix(irradiance[0], irradiance[1], interp.x),
					mix(irradiance[2], irradiance[3], interp.x), interp.y),
				mix(mix(irradiance[4], irradiance[5], interp.x),
					mix(irradiance[6], irradiance[7], interp.x), interp.y), interp.z);

	#ifndef INDIRECT_SPECULAR
		OutputColor =  interpolatedIrradiance * diffuseColor / PI;
	#else
		vec3 interpolatedSpecular = 
			mix(mix(mix(specular[0], specular[1], interp.x),
					mix(specular[2], specular[3], interp.x), interp.y),
				mix(mix(specular[4], specular[5], interp.x),
					mix(specular[6], specular[7], interp.x), interp.y), interp.z);


		OutputColor = interpolatedIrradiance * diffuseColor / PI + interpolatedSpecular * specularColor;
	#endif
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