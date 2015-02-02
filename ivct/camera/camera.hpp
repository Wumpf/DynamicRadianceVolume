#pragma once

#include <ei/matrix.hpp>

class Camera
{
public:
	Camera(const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float hfov = 60.0f, const ei::Vec3& up = ei::Vec3(0.0f, 1.0f, 0.0f));
	virtual ~Camera();

	const ei::Vec3& GetPosition() const  { return m_position; }
	const ei::Vec3& GetLookAt() const  { return m_lookat; }

	virtual void SetAspectRatio(float aspect)				{ m_aspectRatio = aspect; }
	virtual void SetHFov(float hfov)						{ m_hfov = hfov; }
	virtual void SetPosition(const ei::Vec3& position)		{ m_position = position; }
	virtual void SetLookAt(const ei::Vec3& lookat)			{ m_lookat = lookat; }
	
	const ei::Vec3& GetUp() const  { return m_up; }
	const float GetHFov() const  { return m_hfov; }
	const float GetAspectRatio() const  { return m_aspectRatio; }

	ei::Mat4x4 ComputeViewProjection() { return ei::perspectiveGL(m_hfov * (ei::PI / 180.0f), m_aspectRatio, 0.1f, 1000.0f) * ei::camera(m_position, m_lookat, m_up); }

protected:
	ei::Vec3 m_position;
	ei::Vec3 m_lookat;
	ei::Vec3 m_up;
	float m_hfov;
	float m_aspectRatio;
};

