#pragma once

#include <string>
#include <memory>
#include <ei/vector.hpp>

#include "Time/Time.h"

class Model;

class SceneEntity
{
public:
	SceneEntity();
	~SceneEntity();

	void Update(ezTime timeSinceLastUpdate);
	ei::Mat4x4 ComputeWorldMatrix() const;

	/// Returns true if successful
	bool LoadModel(const std::string& modelFilename);
	const std::shared_ptr<Model>& GetModel() const { return m_model; }

	const ei::Vec3& GetPosition() const { return m_position; }
	void SetPosition(const ei::Vec3& position) { m_position = position; }

	float GetScale() const { return m_scale; }
	void SetScale(float scale) { m_scale = scale; }

	const ei::Quaternion& GetOrientation() { return  m_orientation; }
	void SetOrientation(const ei::Quaternion& orientation) { m_orientation = orientation; }

	const ei::Vec3& GetMovementSpeed() { return m_movementSpeed; }
	void SetMovementSpeed(const ei::Vec3& movementSpeed) { m_movementSpeed = movementSpeed; }

	const ei::Vec3& GetRotationSpeed() { return m_rotationSpeed; }
	void SetRotationSpeed(const ei::Vec3& rotationSpeed) { m_rotationSpeed = rotationSpeed; }

private:
	std::shared_ptr<Model> m_model;

	ei::Vec3 m_position;
	float m_scale;
	ei::Quaternion m_orientation;

	ei::Vec3 m_movementSpeed;
	ei::Vec3 m_rotationSpeed;
};

