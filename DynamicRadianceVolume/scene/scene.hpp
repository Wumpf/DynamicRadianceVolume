#pragma once

#include <memory>
#include <vector>
#include <string>

#include <ei/3dtypes.hpp>

#include "light.hpp"
#include "sceneentity.hpp"

#include "Time/Time.h"

namespace gl
{
	class ShaderObject;
	class Texture3D;
	class ScreenAlignedTriangle;
	class SamplerObject;
}
class Camera;

class Scene
{
public:
	Scene();
	~Scene();

	void Update(ezTime timeSinceLastUpdate);

	const std::vector<SceneEntity>& GetEntities() const { return m_entities; }
	std::vector<SceneEntity>& GetEntities() { return m_entities; }

	const std::vector<Light>& GetLights() const { return m_lights; }
	std::vector<Light>& GetLights() { return m_lights; }

	const ei::Box& GetBoundingBox() const { return m_boundingBox; }

private:
	void UpdateBoundingbox();

	std::vector<SceneEntity> m_entities;
	std::vector<Light> m_lights;
	ei::Box m_boundingBox;
};

