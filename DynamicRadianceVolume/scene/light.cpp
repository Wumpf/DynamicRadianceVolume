#include "light.hpp"
#include <Time/Time.h>

void Light::Update(ezTime timeSinceLastUpdate)
{
	float timeSinceLastUpdateSecs = static_cast<float>(timeSinceLastUpdate.GetSeconds());
	position += movementSpeed * timeSinceLastUpdateSecs;

	direction = ei::Mat3x3(ei::Quaternion(rotationSpeed * timeSinceLastUpdateSecs)) * direction;
}