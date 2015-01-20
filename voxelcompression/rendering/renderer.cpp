#include "renderer.hpp"


Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void Renderer::OnScreenResize(ei::UVec2 newResolution)
{
	// Resize buffers etc.
}

void Renderer::SetScene(std::shared_ptr<const Scene> scene)
{
	m_scene = scene;
}

void Renderer::Draw(const Camera& camera)
{

}