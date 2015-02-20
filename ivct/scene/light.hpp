#pragma once

#include <ei/vector.hpp>

struct Light
{
public:
	enum class Type
	{
		SPOT
	};

	Type type;

	ei::Vec3 intensity; // I_0
	ei::Vec3 position;
	ei::Vec3 direction;

	float halfAngle;
};