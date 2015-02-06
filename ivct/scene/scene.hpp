#pragma once

#include <memory>
#include <vector>
#include <string>

#include <ei/3dtypes.hpp>

#include "light.hpp"

namespace gl
{
	class ShaderObject;
	class Texture3D;
	class ScreenAlignedTriangle;
	class SamplerObject;
}
class Model;
class Camera;

class Scene
{
public:
	Scene();
	~Scene();

	void AddModel(const std::string& filename);
	const std::vector<std::shared_ptr<Model>>& GetModels() const { return m_models; }

	void AddLight(const Light& light) { m_lights.push_back(light); }
	const std::vector<Light>& GetLights() const { return m_lights; }

	const ei::Box& GetBoundingBox() const { return m_boundingBox; }

private:
	std::vector<std::shared_ptr<Model>> m_models;
	std::vector<Light> m_lights;
	ei::Box m_boundingBox;
};

