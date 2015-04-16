#pragma once

#include <ei/vector.hpp>

struct Light
{
public:
	Light() : type(Type::SPOT), intensity(10.0f), position(0.0f), direction(0.0f, 0.0f, 1.0f), halfAngle(0.5f), shadowMapResolution(512), 
		normalOffsetShadowBias(0.02f), shadowBias(0.001f){}

	enum class Type
	{
		SPOT
	};

	Type type;

	ei::Vec3 intensity; // I_0
	ei::Vec3 position;
	ei::Vec3 direction;

	float halfAngle;

	unsigned int shadowMapResolution;
	float normalOffsetShadowBias;
	float shadowBias;

	// Near/Farplane for shadow map.
	static const float nearPlane;
	static const float farPlane;
};