#include "camera.hpp"

Camera::Camera(const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float nearPlane, float farPlane, float hfov, const ei::Vec3& up) :
	m_position(position),
	m_lookat(lookat),
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