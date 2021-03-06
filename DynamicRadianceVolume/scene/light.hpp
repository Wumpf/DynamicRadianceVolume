#pragma once

#include <ei/vector.hpp>
#include "camera/cameraspline.hpp"

class ezTime;

struct Light
{
public:
	Light() : type(Type::SPOT), intensity(10.0f), position(0.0f), direction(0.0f, 0.0f, 1.0f), halfAngle(0.5f),
		rsmResolution(1024), rsmReadLod(4),
		normalOffsetShadowBias(0.01f), shadowBias(0.0001f),
		indirectShadowComputationLod(2),
		movementSpeed(0.0f), rotationSpeed(0.0f)
		//followPath(false)
	{}

	/// Applies movementSpeed and rotationSpeed.
	void Update(ezTime timeSinceLastUpdate);

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
	unsigned int rsmReadLod;


	float normalOffsetShadowBias;
	float shadowBias;

	unsigned int indirectShadowComputationLod;

	// Near/Farplane for shadow map.
	static const float nearPlane;
	static const float farPlane;


	ei::Vec3 movementSpeed;
	ei::Vec3 rotationSpeed;

	// Optional path on which this light moves
	//cameraspline path;
	//bool followPath;
};