#pragma once

#include <memory>
#include <vector>
#include <string>

#include <ei/3dtypes.hpp>

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

	const ei::Box& GetBoundingBox() const { return m_boundingBox; }

private:
	std::vector<std::shared_ptr<Model>> m_models;
	ei::Box m_boundingBox;
};

