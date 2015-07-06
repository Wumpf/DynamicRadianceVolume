#version 450 core

// Options:
// Debug address volume cascades.
//#define SHOW_ADDRESSVOL_CASCADES

#include "gbuffer.glsl"
#include "utils.glsl"
#include "globalubos.glsl"
#include "lightingfunctions.glsl"

#define LIGHTCACHEMODE LIGHTCACHEMODE_APPLY
#include "lightcache.glsl"

layout(binding=4) uniform usampler3D VoxelAddressVolume;
layout(binding=6) uniform sampler2D CacheSpecularEnvmap;

in vec2 Texcoord;

out vec3 OutputColor;

vec3 Interp(vec3 x)
{
	return x;
	//return x*x*(3.0 - 2.0*x);
}

vec3 ComputeLightingFromCaches(vec3 worldPosition, vec3 worldNormal, int addressVolumeCascade, vec3 diffuseColor
	#ifdef INDIRECT_SPECULAR
								, vec2 specEnvLookupCoord, vec3 specularColor, float maxHalfSpecularEnvmapPixelSize, float specularEnvmapLod)
	#else
		)
	#endif
{
	vec3 addressCoord = (worldPosition - AddressVolumeCascades[addressVolumeCascade].Min) / AddressVolumeCascades[addressVolumeCascade].WorldVoxelSize;
	ivec3 addressCoord00 = ivec3(addressCoord);

#ifdef SHOW_ADDRESSVOL_CASCADES
	const vec3 cascadeColors[] = { vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, 1, 0), vec3(1, 1, 0), vec3(0, 1, 1)};
	return cascadeColors[addressVolumeCascade] * (mod(length(addressCoord00) / AddressVolumeResolution*5, 1.0) * 0.5 + 0.5);
#endif

	const ivec3 offsets[8] =
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


	// trilinear interpolation
	// Computing all weights here instead of using a bunch of "mix" statements reduces the register pressure! (perf gain was shown empirical!)
	float weights[8];
	{
		vec3 interp = Interp(addressCoord - addressCoord00);
		vec3 interpInv = vec3(1.0) - interp;

		weights[0] = interpInv.x * interpInv.y * interpInv.z;
		weights[1] = interp.x    * interpInv.y * interpInv.z;
		weights[2] = interpInv.x * interp.y    * interpInv.z;
		weights[3] = interp.x    * interp.y    * interpInv.z;
		weights[4] = interpInv.x * interpInv.y * interp.z;
		weights[5] = interp.x    * interpInv.y * interp.z;
		weights[6] = interpInv.x * interp.y    * interp.z;
		weights[7] = interp.x    * interp.y    * interp.z;
	}


	vec3 interpolatedIrradiance = vec3(0);
	vec3 interpolatedSpecular = vec3(0);

	#ifdef INDDIFFUSE_VIA_H
		const float factor0 = 1.0 / (2.0 * PI);
		const float factor1 = sqrt(3.0) / sqrt(2.0 * PI);
		const float factor2 = sqrt(15.0) / sqrt(2.0 * PI);
	#endif



	for(int i=0; i<8; ++i)
	{
		ivec3 cacheSamplePos = addressCoord00 + offsets[i]; //, ivec3(0), ivec3(AddressVolumeResolution-1));
		cacheSamplePos.x += AddressVolumeResolution * addressVolumeCascade;		

		uint cacheAddress = texelFetch(VoxelAddressVolume, cacheSamplePos, 0).r;

		// Check if address is valid. (debug code!)
		/*if(cacheAddress == 0 || cacheAddress == 0xFFFFFFFF || 
			any(lessThan(cacheSamplePos, ivec3(addressVolumeCascade * AddressVolumeResolution, 0, 0))) ||
			any(greaterThanEqual(cacheSamplePos, ivec3(addressVolumeCascade * AddressVolumeResolution + AddressVolumeResolution, AddressVolumeResolution, AddressVolumeResolution))))
		{
			return vec3(1,0,1);
		}*/
		cacheAddress -= 1;

	#ifdef INDIRECT_SPECULAR
		vec2 cacheSpecularEnvmapOffset = vec2(cacheAddress % SpecularEnvmapNumCachesPerDimension, cacheAddress / SpecularEnvmapNumCachesPerDimension);
		vec2 cacheSpecularEnvmapTex = (specEnvLookupCoord + cacheSpecularEnvmapOffset) * SpecularEnvmapPerCacheSize_Texcoord;
		interpolatedSpecular += textureLod(CacheSpecularEnvmap, cacheSpecularEnvmapTex, specularEnvmapLod).rgb * weights[i];
	#endif

		vec3 irradiance = SampleCacheIrradiance(cacheAddress, worldNormal);

		interpolatedIrradiance += irradiance * weights[i];
	}

#ifndef INDIRECT_SPECULAR
	return interpolatedIrradiance * diffuseColor / PI;
#else
	return interpolatedIrradiance * diffuseColor / PI + interpolatedSpecular * specularColor;
#endif
}

void main()
{	
	// Display spec envmap.
	//OutputColor = texelFetch(CacheSpecularEnvmap, ivec2(Texcoord * BackbufferResolution/2), 0).rgb; // TODO
	//return;

	// Get pixel world position.
	float depthBufferDepth = textureLod(GBuffer_Depth, Texcoord, 0.0).r;
	if(depthBufferDepth < 0.00001)
	{
		discard;
	}
	// The cacheGather is performed by a compute shader - Texcoord leads sometimes to slightly different results than its computation
	// Therefore, this "gl_FragCoord.xy / BackbufferResolution" instead of Texcoord is used to ensure same results.
	vec4 worldPosition4D = vec4(gl_FragCoord.xy / BackbufferResolution * 2.0 - vec2(1.0), depthBufferDepth, 1.0) * InverseViewProjection;
	vec3 worldPosition = worldPosition4D.xyz / worldPosition4D.w;

	// Select address volume cascade
	int addressVolumeCascade = ComputeAddressVolumeCascade(worldPosition);

	vec3 worldNormal = UnpackNormal16I(textureLod(GBuffer_Normal, Texcoord, 0.0).rg);

	// Sample material data
	vec3 diffuseColor;
	vec3 baseColor = texture(GBuffer_Diffuse, Texcoord).rgb;
#ifdef INDIRECT_SPECULAR
	
	vec2 roughnessMetalic = texture(GBuffer_RoughnessMetalic, Texcoord).rg;
	vec3 specularColor;
	ComputeMaterialColors(baseColor, roughnessMetalic.y, diffuseColor, specularColor);

	float blinnExponent = RoughnessToBlinnExponent(roughnessMetalic.x);
	float specularEnvmapLod = GetHemisphereLodForBlinnPhongExponent(blinnExponent, SpecularEnvmapPerCacheSize_Texel);
	float maxHalfSpecularEnvmapPixelSize = 0.5 / (pow(2.0, -ceil(specularEnvmapLod)) * SpecularEnvmapPerCacheSize_Texel);

	vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(worldPosition);
	vec2 specEnvLookupCoord = HemisphericalProjection(cacheViewNormal);
	specEnvLookupCoord = clamp(specEnvLookupCoord, vec2(maxHalfSpecularEnvmapPixelSize), vec2(1.0 - maxHalfSpecularEnvmapPixelSize)); // Make sure not to filter pixels from neighboring caches.
#else
	diffuseColor = baseColor;
#endif




	OutputColor = ComputeLightingFromCaches(worldPosition, worldNormal, addressVolumeCascade, diffuseColor
		#ifdef INDIRECT_SPECULAR
											, specEnvLookupCoord, specularColor, maxHalfSpecularEnvmapPixelSize, specularEnvmapLod);
		#else 
				);
		#endif

#ifdef ADDRESSVOL_CASCADE_TRANSITIONS
	float cascadeTransition = ComputeAddressVolumeCascadeTransition(worldPosition, addressVolumeCascade);
	if(cascadeTransition > 0.0 && addressVolumeCascade < NumAddressVolumeCascades-1)
	{
		vec3 secondColor = ComputeLightingFromCaches(worldPosition, worldNormal, addressVolumeCascade+1, diffuseColor
		#ifdef INDIRECT_SPECULAR
											, specEnvLookupCoord, specularColor, maxHalfSpecularEnvmapPixelSize, specularEnvmapLod);
		#else 
				);
		#endif

		OutputColor = mix(OutputColor, secondColor, cascadeTransition);
	}

#endif

	// Test code for "Cache Local View Space"
	/*{
		vec3 cacheWorldPosition = addressCoord00 * AddressVolumeVoxelSize + VolumeWorldMin;
		vec3 cacheViewNormal = worldNormal * ComputeLocalViewSpace(cacheWorldPosition);
		OutputColor = vec3(cacheViewNormal.z < 0 ? 1.0 : -1.0); // Check for negative normals
		OutputColor = cacheViewNormal * 0.5 + 0.5; // Display
	}*/
} 