#include "interactivecamera.hpp"
#include <GLFW/glfw3.h>

InteractiveCamera::InteractiveCamera(GLFWwindow* window, const Camera& camera) :
	Camera(camera.GetPosition(), camera.GetDirection(), camera.GetAspectRatio(), camera.GetNearPlane(), camera.GetFarPlane(), camera.GetHFov(), camera.GetUp()),
	m_window(m_window),
	m_rotSpeed(0.01f),
	m_moveSpeed(4.0f)
{
}

void InteractiveCamera::Reset(const Camera& camera)
{
	m_position = camera.GetPosition();
	m_aspectRatio = camera.GetAspectRatio();
	m_hfov = camera.GetHFov();
	m_up = camera.GetUp();
}

InteractiveCamera::InteractiveCamera(GLFWwindow* window, const ei::Vec3& position, const ei::Vec3& lookat, float aspectRatio, float nearPlane, float farPlane, float hfov, const ei::Vec3& up) :
	Camera(position, lookat, aspectRatio, nearPlane, farPlane, hfov, up),
	m_window(window),
	m_rotSpeed(0.01f),
	m_moveSpeed(4.0f)
{
}

void InteractiveCamera::Update(ezTime timeSinceLastFrame)
{
	double newMousePosX, newMousePosY;
	glfwGetCursorPos(m_window, &newMousePosX, &newMousePosY);

	if(glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT))
	{
		double rotY = asin(m_direction.y);
		double rotX = atan2(m_direction.x, m_direction.z);
		rotX += -m_rotSpeed * (newMousePosX - m_lastMousePosX);
		rotY += m_rotSpeed * (newMousePosY - m_lastMousePosY);

		float scaledMoveSpeed = m_moveSpeed;
		if (glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) || glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT))
			scaledMoveSpeed *= 10.0f;

		float forward = (glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) ? 1.0f : 0.0f;
		float back = (glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) ? 1.0f : 0.0f;
		float left = (glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) ? 1.0f : 0.0f;
		float right = (glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) ? 1.0f : 0.0f;

		m_direction = ei::Vec3(static_cast<float>(sin(rotX) * cos(rotY)), static_cast<float>(sin(rotY)), static_cast<float>(cos(rotX) * cos(rotY)));
		ei::Vec3 cameraLeft = ei::cross(m_direction, ei::Vec3(0, 1, 0));

		ei::Vec3 oldPosition = m_position;
		m_position = m_position + ((forward - back) * m_direction + (right - left) * cameraLeft) * scaledMoveSpeed * static_cast<float>(timeSinceLastFrame.GetSeconds());
	}

	m_lastMousePosX = newMousePosX;
	m_lastMousePosY = newMousePosY;
}