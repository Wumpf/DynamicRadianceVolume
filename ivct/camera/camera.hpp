#pragma once

#include <ei/vector.hpp>

class Camera
{
public:
	Camera(const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float nearPlane, float farPlane, float hfov = 60.0f, const ei::Vec3& up = ei::Vec3(0.0f, 1.0f, 0.0f));
	virtual ~Camera();

	const ei::Vec3& GetPosition() const  { return m_position; }
	const ei::Vec3& GetLookAt() const  { return m_lookat; }
	ei::Vec3 GetDirection() const  { return ei::normalize(m_lookat - m_position); }

	virtual void SetAspectRatio(float aspect)				{ m_aspectRatio = aspect; }
	virtual void SetHFov(float hfov)						{ m_hfov = hfov; }
	virtual void SetPosition(const ei::Vec3& position)		{ m_position = position; }
	virtual void SetLookAt(const ei::Vec3& lookat)			{ m_lookat = lookat; }
	void SetNearPlane(float nearPlane)						{ m_nearPlane = nearPlane; }
	void SetFarPlane(float farPlane)						{ m_farPlane = farPlane; }

	const ei::Vec3& GetUp() const  { return m_up; }
	float GetHFov() const  { return m_hfov; }
	float GetAspectRatio() const  { return m_aspectRatio; }
	float GetNearPlane() const { return m_nearPlane; }
	float GetFarPlane() const { return m_farPlane; }

	/// Computes perspective projection matrix in <b>DIRECTX</b> style with <b>flipped near & far plane</b>.
	ei::Mat4x4 ComputeProjectionMatrix() const { return ei::perspectiveDX(m_hfov * (ei::PI / 180.0f), m_aspectRatio, m_farPlane, m_nearPlane); }

	ei::Mat4x4 ComputeViewMatrix() const { return ei::camera(m_position, m_lookat, m_up); }

protected:
	ei::Vec3 m_position;
	ei::Vec3 m_lookat;
	ei::Vec3 m_up;
	float m_hfov;
	float m_aspectRatio;
	float m_nearPlane;
	float m_farPlane;
};

