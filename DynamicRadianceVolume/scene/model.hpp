#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ei/vector.hpp>
#include <ei/3dtypes.hpp>

#include <glhelper/gl.hpp>
#include <glhelper/texture2d.hpp>

#include <json/json.h>

namespace gl
{
	class Buffer;
	class VertexArrayObject;
}

class Model
{
public:
	/// Loads a model from a given filename.
	/// Checks first if there is a json with the model information. If not or if not valid the given filename will be loaded.
	/// 
	/// \param writeRawIfNotFound
	///		If true and no json was found, a new json and vertex buffer file will be written.
	/// \return nullptr if the path is invalid or the file could not be loaded.
	static std::shared_ptr<Model> FromFile(const std::string& filename, bool writeRawIfNotFound = true);

	~Model();

	const std::string& GetOriginFilename() { return m_originFilename; }

	struct Mesh
	{
		Mesh() : startIndex(0), numIndices(0), alphaTesting(false) {}
		
		/// Loads textures from origin values.
		/// \param directory
		///		Directory to which the texture filenames are relative.
		void LoadTextures(const std::string& directory);

		unsigned int startIndex;
		unsigned int numIndices;

		std::shared_ptr<gl::Texture2D> diffuse;
		std::shared_ptr<gl::Texture2D> normalmap;	// Tangent space normals RGB -> XZY*2.0 - 1.0
		std::shared_ptr<gl::Texture2D> roughnessMetallic; // Combined texture of roughness (R) and metallic values (G)
		
		bool alphaTesting;
		bool doubleSided; // Usually set to true, if alphaTesting enabled.

		Json::Value diffuseOrigin;
		Json::Value normalmapOrigin;
		Json::Value roughnessOrigin;
		Json::Value metallicOrigin;
	};

	struct Vertex
	{
		ei::Vec3 position;
		ei::Vec3 normal;
		ei::Vec4 tangent; // The 4th component determines the handedness of the bitangent
		ei::Vec2 texcoord;
	};

	unsigned int GetNumTriangles() const { return m_numTriangles; }
	unsigned int GetNumVertices() const { return m_numVertices; }
	const std::vector<Mesh>& GetMeshes() const { return m_meshes; }

	const ei::Box& GetBoundingBox() const { return m_boundingBox; }

	static void CreateVAO();
	static void DestroyVAO();
	static void BindVAO();

	/// Binds vertex and index buffer.
	void BindBuffers();

private:
	Model(const std::string& originFilename);

	void SaveRaw(const std::string& filename, const Vertex* rawVertexData, const std::uint32_t* rawIndexData) const;
	/// \param filename
	///		Filename of the json file.
	/// \param directory
	///		Directory used for all relative paths (raw, textures)
	static std::shared_ptr<Model> LoadFromRaw(const std::string& filename, const std::string& directory);

	/// \param filename
	///		Filename of the model file.
	/// \param directory
	///		Directory used for all relative paths (textures)
	static std::shared_ptr<Model> LoadViaAssimp(const std::string& filename, const std::string& directory, 
												std::unique_ptr<Vertex[]>& outVertices, std::unique_ptr<std::uint32_t[]>& outIndices);

	static std::unique_ptr<gl::VertexArrayObject> m_vertexArrayObject;

	const std::string m_originFilename;

	std::vector<Mesh> m_meshes;

	unsigned int m_numTriangles;
	unsigned int m_numVertices;

	std::unique_ptr<gl::Buffer> m_vertexBuffer;
	std::unique_ptr<gl::Buffer> m_indexBuffer;

	ei::Box m_boundingBox;

	static const unsigned int m_rawModelVersion;
};

