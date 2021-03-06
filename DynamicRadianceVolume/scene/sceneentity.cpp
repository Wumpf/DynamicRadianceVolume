#include "sceneentity.hpp"
#include "model.hpp"

SceneEntity::SceneEntity() :
	m_model(),
	m_position(0.0f),
	m_scale(1.0f),
	m_orientation(ei::qidentity()),

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

void SceneEntity::Update(ezTime timeSinceLastUpdate)
{
	float timeSinceLastUpdateSecs = static_cast<float>(timeSinceLastUpdate.GetSeconds());
	m_position += m_movementSpeed * timeSinceLastUpdateSecs;
	m_orientation *= ei::Quaternion(m_rotationSpeed * timeSinceLastUpdateSecs);
}

ei::Mat4x4 SceneEntity::ComputeWorldMatrix() const
{
	return ei::translation(m_position) * ei::rotationH(m_orientation) * ei::scalingH(m_scale);
}