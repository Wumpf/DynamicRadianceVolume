#include "scene.hpp"
#include "model.hpp"


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

void Scene::AddModel(const std::string& filename)
{
	LOG_INFO("Loading " << filename << " ...");
	std::shared_ptr<Model> model = Model::FromFile(filename);
	if (model)
	{
		m_models.push_back(model);
		m_boundingBox = ei::Box(m_boundingBox, model->GetBoundingBox());
	}
}
