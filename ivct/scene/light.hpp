#pragma once

#include <ei/vector.hpp>

struct Light
{
public:
	enum class Type
	{
		SPOT
	};

	ei::Vec3 position;
	ei::Vec3 direction;
	float halfAngle;
};