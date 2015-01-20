#pragma once

#include <memory>
#include <ei/matrix.hpp>
#include "camera/camera.hpp"

// Directly included for convenience: Not having uniform buffer as pointer enables nicer [] syntax for vars.
#include <glhelper/uniformbuffer.hpp>

namespace gl
{
	class FramebufferObject;
	class ShaderObject;
}
class Scene;

class Renderer
{
public:
	Renderer();
	~Renderer();

	enum class Mode
	{
		NORMAL
	};

	void OnScreenResize(ei::UVec2 newResolution);

	void SetScene(std::shared_ptr<const Scene> scene);

	void Draw(const Camera& camera);

private:
	std::shared_ptr<const Scene> m_scene;
	
	Mode m_currentMode;
	std::unique_ptr<gl::ShaderObject> m_debugShader;

	gl::UniformBufferView m_perFrameUniformBuffer;
};

