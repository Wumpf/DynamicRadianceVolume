#pragma once

#include <ei/vector.hpp>

struct Light
{
public:
	Light() : type(Type::SPOT), intensity(10.0f), position(0.0f), direction(0.0f, 0.0f, 1.0f), halfAngle(0.5f),
		rsmResolution(32), shadowMapResolution(1024),
		normalOffsetShadowBias(0.1f), shadowBias(0.008f),
		indirectShadowComputationLod(2), 
		indirectShadowMinHalfConeAngle(0.05f) // about 2.9 degree half angle (~5.7 full cone)
	{} 

	enum class Type
	{
		SPOT
	};

	Type type;

	ei::Vec3 intensity; // I_0
	ei::Vec3 position;
	ei::Vec3 direction;

	float halfAngle;

	unsigned int rsmResolution; // Only Pow2 resolutions are allowed
	unsigned int shadowMapResolution;
	float normalOffsetShadowBias;
	float shadowBias;

	unsigned int indirectShadowComputationLod;
	float indirectShadowMinHalfConeAngle;

	// Near/Farplane for shadow map.
	static const float nearPlane;
	static const float farPlane;
};