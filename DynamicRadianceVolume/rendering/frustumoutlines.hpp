#pragma once

#include <memory>

namespace gl
{
	class Buffer;
	class VertexArrayObject;
	class ShaderObject;
}
class Camera;

class FrustumOutlines
{
public:
	FrustumOutlines();
	~FrustumOutlines();

	/// Updates outlines with given camera
	void Update(const Camera& camera);

	/// Draws outlines.
	/// Assumes that globalubos.glsl / PerFrame UBO is bound and up to date!
	void Draw();

private:
	std::unique_ptr<gl::ShaderObject> m_shader;
	std::unique_ptr<gl::VertexArrayObject> m_vao;
	std::unique_ptr<gl::Buffer> m_vertices;
	std::unique_ptr<gl::Buffer> m_indices;
};

