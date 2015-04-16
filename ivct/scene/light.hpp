#pragma once

#include <ei/vector.hpp>

struct Light
{
public:
	Light() : type(Type::SPOT), intensity(10.0f), position(0.0f), direction(0.0f, 0.0f, 1.0f), halfAngle(0.5f), shadowMapResolution(128) {}

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
};