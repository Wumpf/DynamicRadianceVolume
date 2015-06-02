#include "frustumoutlines.hpp"

#include "../camera/camera.hpp"

#include <glhelper/buffer.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/vertexarrayobject.hpp>
#include <glhelper/statemanagement.hpp>

FrustumOutlines::FrustumOutlines() :
	m_shader(new gl::ShaderObject("DebugLines"))
{
	std::int32_t indices[4 * 8] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 3, 7, 2, 6, 1, 5
	};

	m_vao.reset(new gl::VertexArrayObject({ gl::VertexArrayObject::Attribute(gl::VertexArrayObject::Attribute::Type::FLOAT, 3) }));
	m_vertices = std::make_unique<gl::Buffer>(sizeof(ei::Vec3) * 8, gl::Buffer::UsageFlag::MAP_WRITE);
	m_indices = std::make_unique<gl::Buffer>(sizeof(std::int32_t) * 4 * 8, gl::Buffer::UsageFlag::IMMUTABLE, indices);
	

	m_shader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/debuglines.vert");
	m_shader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/debuglines.frag");
	m_shader->CreateProgram();
}

FrustumOutlines::~FrustumOutlines()
{
}

void FrustumOutlines::Update(const Camera& camera)
{
	auto inverseViewProjection = ei::invert(camera.ComputeProjectionMatrix() * camera.ComputeViewMatrix());

	ei::Vec3* vertices = static_cast<ei::Vec3*>(m_vertices->Map(gl::Buffer::MapType::WRITE, gl::Buffer::MapWriteFlag::INVALIDATE_BUFFER));
	vertices[0] = ei::transform(ei::Vec3(1.0f, -1.0f, 1.0f), inverseViewProjection);
	vertices[1] = ei::transform(ei::Vec3(1.0f, 1.0f, 1.0f), inverseViewProjection);
	vertices[2] = ei::transform(ei::Vec3(-1.0f, 1.0f, 1.0f), inverseViewProjection);
	vertices[3] = ei::transform(ei::Vec3(-1.0f, -1.0f, 1.0f), inverseViewProjection);
	vertices[4] = ei::transform(ei::Vec3(1.0f, -1.0f, 0.0f), inverseViewProjection);
	vertices[5] = ei::transform(ei::Vec3(1.0f, 1.0f, 0.0f), inverseViewProjection);
	vertices[6] = ei::transform(ei::Vec3(-1.0f, 1.0f, 0.0f), inverseViewProjection);
	vertices[7] = ei::transform(ei::Vec3(-1.0f, -1.0f, 0.0f), inverseViewProjection);
	m_vertices->Unmap();
}

void FrustumOutlines::Draw()
{
	m_vao->Bind();
	m_shader->Activate();
	m_vertices->BindVertexBuffer(0, 0, sizeof(ei::Vec3));
	m_indices->BindIndexBuffer();
	GL_CALL(glDrawElements, GL_LINES, 4 * 8, GL_UNSIGNED_INT, nullptr);
}