#include "sceneentity.hpp"
#include "model.hpp"

SceneEntity::SceneEntity() :
	m_model(),
	m_position(0.0f),
	m_scale(1.0f),
	m_orientation(0, 0, 0),

	m_movementSpeed(0),
	m_rotationSpeed(0)
{}

SceneEntity::~SceneEntity()
{}

bool SceneEntity::LoadModel(const std::string& modelFilename)
{
	LOG_INFO("Loading " << modelFilename << " ...");
	m_model = Model::FromFile(modelFilename);
	return m_model != nullptr;
}

ei::Mat4x4 SceneEntity::ComputeWorldMatrix() const
{
	return ei::translation(m_position) * ei::rotationH(m_orientation);
}