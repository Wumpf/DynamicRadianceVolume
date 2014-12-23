#include "camera.hpp"

Camera::Camera(const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float hfov, const ei::Vec3& up) :
	m_position(position),
	m_lookat(lookat),
	m_aspectRatio(aspectRatio),
	m_hfov(hfov),
	m_up(up)
{
}

Camera::~Camera(void)
{
}