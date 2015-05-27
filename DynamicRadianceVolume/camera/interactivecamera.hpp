#pragma once

#include "camera.hpp"
#include "../Time/Time.h"

struct GLFWwindow;

/// Interactive "ego"-camera, using GLFW functions.
class InteractiveCamera : public Camera
{
public:
	InteractiveCamera(GLFWwindow* window, const Camera& camera);
	InteractiveCamera(GLFWwindow* window, const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float nearPlane, float farPlane, 
							float hfov = 60.0f, const ei::Vec3& up = ei::Vec3(0.0f, 1.0f, 0.0f));

	void Reset(const Camera& camera);

	void Update(ezTime timeSinceLastFrame);

	void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
	float GetMoveSpeed() const { return m_moveSpeed; }

private:
	GLFWwindow* m_window;
	float m_rotSpeed, m_moveSpeed;
	double m_lastMousePosX, m_lastMousePosY;
};

