#include "model.hpp"
#include "utilities/assert.hpp"
#include "utilities/logger.hpp"

#include <glhelper/buffer.hpp>

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>

gl::VertexArrayObjectId Model::m_vertexArrayObject = 0;

Model::Model() :
	m_numTriangles(0),
	m_numVertices(0)
{
}

Model::~Model()
{
	GL_CALL(glDeleteVertexArrays, 1, &m_vertexArrayObject);
}

std::shared_ptr<Model> Model::FromFile(const std::string& filename)
{
	// Ignore line/point primitives
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

	const aiScene* scene = importer.ReadFile(filename,
		//aiProcess_CalcTangentSpace |
		aiProcess_MakeLeftHanded |
		//aiProcess_PreTransformVertices |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_ValidateDataStructure |
		aiProcess_SortByPType |
		aiProcess_RemoveRedundantMaterials |
		//aiProcess_FixInfacingNormals	|
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords |
		aiProcess_TransformUVCoords |
		aiProcess_ImproveCacheLocality |
		aiProcess_JoinIdenticalVertices
		//aiProcess_FlipUVs
		);

	if(!scene)
	{
		LOG_ERROR("Failed to load model file " + filename + " reason: " + importer.GetErrorString());
		return nullptr;
	}

	std::shared_ptr<Model> output(new Model);

	// Count tris/vertices
	for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		if(scene->mMeshes[i])
		{
			auto& mesh = *scene->mMeshes[i];
			Assert(mesh.HasNormals(), "Mesh " << mesh.mName.C_Str() << "(" << i << ") of model " << filename << " has no normals!");
			output->m_numVertices += mesh.mNumVertices;
			output->m_numTriangles += mesh.mNumFaces;
		}
	}
	
	// Load vertices
	std::unique_ptr<Vertex[]> vertices(new Vertex[output->m_numVertices]);
	Vertex* currentVertex = vertices.get();
	for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		if(scene->mMeshes[i])
		{
			auto& mesh = *scene->mMeshes[i];
			for(unsigned int v = 0; v < mesh.mNumVertices; ++v)
			{
				memcpy(&currentVertex->position, &mesh.mVertices[v], sizeof(float) * 3);
				memcpy(&currentVertex->normal, &mesh.mNormals[v], sizeof(float) * 3);
				++currentVertex;
			}

			if(mesh.HasTextureCoords(0))
			{
				currentVertex = currentVertex - mesh.mNumVertices;
				for(unsigned int v = 0; v < mesh.mNumVertices; ++v)
				{
					memcpy(&currentVertex->texcoord, &mesh.mTextureCoords[0][v], sizeof(float) * 2);
					++currentVertex;
				}
			}
			else
			{
				LOG_WARNING("Mesh " << mesh.mName.C_Str() << "(" << i << ") of model " << filename << " has no texture coordinates!");
			}
		}
	}
	output->m_vertexBuffer.reset(new gl::Buffer(sizeof(Vertex) * output->m_numVertices, gl::Buffer::Usage::IMMUTABLE, vertices.get()));
	vertices.release();

	// Load indices - Currently only 32bit indices
	std::unique_ptr<std::uint32_t[]> indices(new std::uint32_t[output->m_numTriangles * 3]);
	std::uint32_t indexOffset = 0;
	std::uint32_t* currentIndex = indices.get();
	for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		if(scene->mMeshes[i])
		{
			auto& mesh = *scene->mMeshes[i];
			for(unsigned int f = 0; f < mesh.mNumFaces; ++f)
			{
				Assert(mesh.mFaces[f].mNumIndices == 3, "Mesh contains non-triangles!");

				*currentIndex = mesh.mFaces[f].mIndices[0] + indexOffset;
				Assert(*currentIndex < output->m_numTriangles * 3, "Vertex index is out of range!");
				currentIndex++;

				*currentIndex = mesh.mFaces[f].mIndices[1] + indexOffset; 
				Assert(*currentIndex < output->m_numTriangles * 3, "Vertex index is out of range!");
				currentIndex++;

				*currentIndex = mesh.mFaces[f].mIndices[2] + indexOffset;
				Assert(*currentIndex < output->m_numTriangles * 3, "Vertex index is out of range!");
				currentIndex++;
			}
			indexOffset += mesh.mNumVertices;
		}
	}
	output->m_indexBuffer.reset(new gl::Buffer(sizeof(std::uint32_t) * output->m_numTriangles * 3, gl::Buffer::Usage::IMMUTABLE, indices.get()));
	indices.release();

	importer.FreeScene();

	return output;
}

void Model::CreateVAO()
{
	DestroyVAO();

	// Thanks to OpenGL 4.3 + bindless we are able to define a vertex array object very similar to a directX vertex declaration.
	// Later on we need to use glBindVertexBuffer instead of the classic glBindBuffer

	GL_CALL(glCreateVertexArrays, 1, &m_vertexArrayObject);
	
	// Activate attributes
	GL_CALL(glEnableVertexArrayAttrib, m_vertexArrayObject, 0);
	GL_CALL(glEnableVertexArrayAttrib, m_vertexArrayObject, 1);
	GL_CALL(glEnableVertexArrayAttrib, m_vertexArrayObject, 2);

	// Define attributes to be from vertex buffer 0.
	GL_CALL(glVertexArrayAttribBinding, m_vertexArrayObject, 0, 0);
	GL_CALL(glVertexArrayAttribBinding, m_vertexArrayObject, 1, 0);
	GL_CALL(glVertexArrayAttribBinding, m_vertexArrayObject, 2, 0);

	// Vertex attributes.
	GL_CALL(glVertexArrayAttribFormat, m_vertexArrayObject, 0, 3, GL_FLOAT, GL_FALSE, static_cast<GLuint>(0)); // position
	GL_CALL(glVertexArrayAttribFormat, m_vertexArrayObject, 1, 3, GL_FLOAT, GL_FALSE, static_cast<GLuint>(sizeof(float) * 3)); // normal
	GL_CALL(glVertexArrayAttribFormat, m_vertexArrayObject, 2, 2, GL_FLOAT, GL_FALSE, static_cast<GLuint>(sizeof(float) * 6)); // texcoord
}

void Model::DestroyVAO()
{
	if(m_vertexArrayObject != 0)
		GL_CALL(glDeleteVertexArrays,1, &m_vertexArrayObject);
}

void Model::BindVAO()
{
	GL_CALL(glBindVertexArray, m_vertexArrayObject);
}

void Model::BindBuffers()
{
	GL_CALL(glBindVertexBuffer, 0, m_vertexBuffer->GetBufferId(), 0, static_cast<GLsizei>(sizeof(Vertex))); // TODO: Move into buffer
	GL_CALL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer->GetBufferId()); // TODO: Move into buffer
}