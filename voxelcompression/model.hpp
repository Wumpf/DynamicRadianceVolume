#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ei/matrix.hpp>

#include <glhelper/gl.hpp>

namespace gl
{
	class Buffer;
	class VertexArrayObject;
}

class Model
{
public:
	/// Loads a model from a given filename.
	/// \return nullptr if the path is invalid or the file could not be loaded.
	static std::shared_ptr<Model> FromFile(const std::string& filename);

	~Model();

	struct Mesh
	{
	};

	struct Vertex
	{
		ei::Vec3 position;
		ei::Vec3 normal;
		ei::Vec2 texcoord;
	};

	unsigned int GetNumTriangles() const { return m_numTriangles; }
	unsigned int GetNumVertices() const { return m_numVertices; }

	static void CreateVAO();
	static void DestroyVAO();
	static void BindVAO();

	/// Binds vertex and index buffer.
	void BindBuffers();

private:
	Model();

	/// Creates a vertex/index buffer independent VAO (vertex format only)
	static gl::VertexArrayObjectId m_vertexArrayObject;

	std::vector<Mesh> meshes;

	unsigned int m_numTriangles;
	unsigned int m_numVertices;

	std::unique_ptr<gl::Buffer> m_vertexBuffer;
	std::unique_ptr<gl::Buffer> m_indexBuffer;
};

