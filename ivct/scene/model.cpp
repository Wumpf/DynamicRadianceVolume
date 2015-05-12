#include "model.hpp"
#include "texturemanager.hpp"

#include "utilities/assert.hpp"
#include "utilities/logger.hpp"
#include "utilities/pathutils.hpp"

#include <glhelper/buffer.hpp>
#include <glhelper/vertexarrayobject.hpp>

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>

std::unique_ptr<gl::VertexArrayObject> Model::m_vertexArrayObject;

Model::Model(const std::string& originFilename) :
	m_originFilename(originFilename),
	m_numTriangles(0),
	m_numVertices(0)
{
	m_boundingBox.min = ei::Vec3(std::numeric_limits<float>::max());
	m_boundingBox.max = ei::Vec3(std::numeric_limits<float>::min());
}

Model::~Model()
{
}

std::shared_ptr<Model> Model::FromFile(const std::string& filename)
{
	std::string directory(PathUtils::GetDirectory(PathUtils::CanonicalizePath(filename)));

	// Ignore line/point primitives
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_CalcTangentSpace |
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
		aiProcess_JoinIdenticalVertices |
		aiProcess_FlipUVs |
		aiProcess_OptimizeMeshes
		);

	if(!scene)
	{
		LOG_ERROR("Failed to load model file " + filename + " reason: " + importer.GetErrorString());
		return nullptr;
	}

	std::shared_ptr<Model> output(new Model(filename));

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
	output->m_meshes.resize(scene->mNumMeshes);

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
				memcpy(&currentVertex->tangent, &mesh.mTangents[v], sizeof(float) * 3);
				
				// Retrieve bitangent handedness - see also http://www.terathon.com/code/tangent.html
				ei::Vec3 bitangent(mesh.mBitangents[v].x, mesh.mBitangents[v].y, mesh.mBitangents[v].z);
				currentVertex->tangent.w = (ei::dot(ei::cross(currentVertex->normal, ei::Vec3(currentVertex->tangent.x, currentVertex->tangent.x, currentVertex->tangent.x)), bitangent) < 0.0f) ? -1.0f : 1.0f;
				
				output->m_boundingBox.min = ei::min(output->m_boundingBox.min, currentVertex->position);
				output->m_boundingBox.max = ei::max(output->m_boundingBox.max, currentVertex->position);
				++currentVertex;
			}

			currentVertex = currentVertex - mesh.mNumVertices;
			if(mesh.HasTextureCoords(0))
			{
				for(unsigned int v = 0; v < mesh.mNumVertices; ++v)
				{
					memcpy(&currentVertex->texcoord, &mesh.mTextureCoords[0][v], sizeof(float) * 2);
					++currentVertex;
				}
			}
			else
			{
				for (unsigned int v = 0; v < mesh.mNumVertices; ++v)
				{
					currentVertex->texcoord = ei::Vec2(0.5f);
					++currentVertex;
				}
			}
		}
	}
	output->m_vertexBuffer.reset(new gl::Buffer(sizeof(Vertex) * output->m_numVertices, gl::Buffer::UsageFlag::IMMUTABLE, vertices.get()));
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

			output->m_meshes[i].numIndices = mesh.mNumFaces * 3;
			output->m_meshes[i].startIndex = static_cast<unsigned int>(currentIndex - indices.get());
			
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
	output->m_indexBuffer.reset(new gl::Buffer(sizeof(std::uint32_t) * output->m_numTriangles * 3, gl::Buffer::UsageFlag::IMMUTABLE, indices.get()));
	indices.release();

	// Load textures / material properties
	if (scene->HasMaterials())
	{
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			if (scene->mMeshes[i])
			{
				auto& mesh = *scene->mMeshes[i];

				{
					aiString diffuseTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &diffuseTexture);
					if (diffuseTexture.length)
					{
						std::string textureFilename = PathUtils::AppendPath(directory, diffuseTexture.C_Str());
						output->m_meshes[i].diffuse = TextureManager::GetInstance().GetDiffuse(textureFilename);
					}
					else
					{
						aiColor3D aiColor = aiColor3D(0.0f, 0.0f, 0.0f);
						scene->mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor);
						ei::Vec3 diffuseColor(aiColor.r, aiColor.g, aiColor.b);
						output->m_meshes[i].diffuse = TextureManager::GetInstance().GetDiffuse(diffuseColor);
					}
				}

				{
					aiString normalTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_NORMALS, 0, &normalTexture);
					if (normalTexture.length)
					{
						std::string textureFilename = PathUtils::AppendPath(directory, normalTexture.C_Str());
						output->m_meshes[i].normalmap = TextureManager::GetInstance().GetNormalmap(textureFilename);
					}
					else
					{
						output->m_meshes[i].normalmap = TextureManager::GetInstance().GetDefaultNormal();
					}
				}

				{
					aiString metallicTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_SPECULAR, 0, &metallicTexture);
					aiString roughnessTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_SHININESS, 0, &roughnessTexture);

					if (roughnessTexture.length)
					{
						std::string roughnessTextureFilename = PathUtils::AppendPath(directory, roughnessTexture.C_Str());
						std::string metallicTextureFilename;
						if (metallicTexture.length)
							metallicTextureFilename = PathUtils::AppendPath(directory, metallicTexture.C_Str());
						
						output->m_meshes[i].roughnessMetalic = TextureManager::GetInstance().GetRoughnessMetallic(roughnessTextureFilename, metallicTextureFilename);
					}
					else
					{
						output->m_meshes[i].roughnessMetalic = TextureManager::GetInstance().GetDefaultRoughnessMetallic();
					}
				}
			}
		}
	}

	importer.FreeScene();

	return output;
}

void Model::CreateVAO()
{
	DestroyVAO();

	using Attribute = gl::VertexArrayObject::Attribute;
	m_vertexArrayObject.reset(new gl::VertexArrayObject({
		Attribute(Attribute::Type::FLOAT, 3),
		Attribute(Attribute::Type::FLOAT, 3),
		Attribute(Attribute::Type::FLOAT, 3),
		Attribute(Attribute::Type::FLOAT, 1),
		Attribute(Attribute::Type::FLOAT, 2),
	}));
}

void Model::DestroyVAO()
{
	m_vertexArrayObject.reset();
}

void Model::BindVAO()
{
	m_vertexArrayObject->Bind();
}

void Model::BindBuffers()
{
	m_vertexBuffer->BindVertexBuffer(0, 0, static_cast<GLsizei>(sizeof(Vertex)));
	m_indexBuffer->BindIndexBuffer();
}