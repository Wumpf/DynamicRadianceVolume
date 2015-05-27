#include "camera.hpp"

Camera::Camera(const ei::Vec3& position, const ei::Vec3& direction, float aspectRatio, float nearPlane, float farPlane, float hfov, const ei::Vec3& up) :
	m_position(position),
	m_direction(ei::normalize(direction)),
	m_aspectRatio(aspectRatio),
	m_hfov(hfov),
	m_up(up),
	m_nearPlane(nearPlane), 
	m_farPlane(farPlane)
{
}

Camera::~Camera(void)
{
}