#pragma once

#include <memory>
#include <vector>
#include <string>

// Directly included for convenience: Not having uniform buffer as pointer enables nicer [] syntax for vars.
#include <glhelper/uniformbuffer.hpp>

namespace gl
{
	class ShaderObject;
}
class Model;
class Camera;

class Scene
{
public:
	Scene();
	~Scene();

	void AddModel(const std::string& filename);

	void Draw(Camera& camera);

private:
	std::unique_ptr<gl::ShaderObject> m_simpleShader;
	gl::UniformBufferView m_perFrameUniformBuffer;

	std::vector<std::shared_ptr<Model>> models;
};

