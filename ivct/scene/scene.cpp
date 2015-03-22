#include "scene.hpp"
#include "sceneentity.hpp"
#include "model.hpp"
#include <ei/3dfunctions.hpp>

Scene::Scene() :
	m_boundingBox()
{
	Model::CreateVAO();

	m_boundingBox.min = ei::Vec3(std::numeric_limits<float>::max());
	m_boundingBox.max = ei::Vec3(std::numeric_limits<float>::min());
}

Scene::~Scene()
{
	Model::DestroyVAO();
}

void Scene::UpdateBoundingbox()
{
	m_boundingBox.min = ei::Vec3(std::numeric_limits<float>::max());
	m_boundingBox.max = ei::Vec3(std::numeric_limits<float>::min());

	for (auto& it : m_entities)
	{
		if (it.GetModel())
		{
			m_boundingBox = ei::Box(m_boundingBox, it.GetModel()->GetBoundingBox());
		}
	}
}