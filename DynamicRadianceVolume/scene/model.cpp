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
const unsigned int Model::m_rawModelVersion = 0;

Model::Model(const std::string& originFilename) :
	m_originFilename(PathUtils::CanonicalizePath(originFilename)),
	m_numTriangles(0),
	m_numVertices(0)
{
	m_boundingBox.min = ei::Vec3(std::numeric_limits<float>::max());
	m_boundingBox.max = ei::Vec3(std::numeric_limits<float>::min());
}

Model::~Model()
{
}

std::shared_ptr<Model> Model::FromFile(const std::string& filename, bool writeRawIfNotFound)
{
	std::string directory(PathUtils::GetDirectory(PathUtils::CanonicalizePath(filename)));
	auto endingPos = filename.find_last_of('.');

	std::ifstream fileopenCheck;
	std::shared_ptr<Model> output;

	std::string directoryRaw = filename.substr(0, endingPos) + ".json";
	fileopenCheck.open(directoryRaw.c_str());
	bool rawAvailable = fileopenCheck.is_open();
	fileopenCheck.close();
	if(rawAvailable)
	{
		output = LoadFromRaw(directoryRaw, directory);
		rawAvailable = output != nullptr;
	}


	std::unique_ptr<Vertex[]> rawVertexData;
	std::unique_ptr<std::uint32_t[]> rawIndexData;
	if(!output)
	{
		output = LoadViaAssimp(filename, directory, rawVertexData, rawIndexData);
	}

	if(output && writeRawIfNotFound && !rawAvailable)
	{
		output->SaveRaw(directoryRaw, rawVertexData.get(), rawIndexData.get());
	}

	return output;
}

void Model::SaveRaw(const std::string& filename, const Vertex* rawVertexData, const std::uint32_t* rawIndexData) const
{
	std::string rawbufferFilename = PathUtils::CanonicalizePath(filename.substr(0, filename.find_last_of('.')) + ".rawbuffer");

	Json::Value jsonRoot;

	// Basic header
	Json::Value header;
	header["version"] = m_rawModelVersion;
	header["originFilename"] = PathUtils::GetFilename(m_originFilename);
	header["rawbufferFilename"] = PathUtils::GetFilename(rawbufferFilename);
	header["numVertices"] = m_numVertices;
	header["numTriangles"] = m_numTriangles;
	jsonRoot["header"] = header;

	// Bounding box
	Json::Value boundingBox;
	boundingBox["min"][0] = m_boundingBox.min[0];
	boundingBox["min"][1] = m_boundingBox.min[1];
	boundingBox["min"][2] = m_boundingBox.min[2];
	boundingBox["max"][0] = m_boundingBox.max[0];
	boundingBox["max"][1] = m_boundingBox.max[1];
	boundingBox["max"][2] = m_boundingBox.max[2];
	jsonRoot["boundingBox"] = boundingBox;

	// Meshes
	for (unsigned int meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx)
	{
		const Mesh& mesh = m_meshes[meshIdx];
		Json::Value jsonMesh;

		jsonMesh["startIndex"] = mesh.startIndex;
		jsonMesh["numIndices"] = mesh.numIndices;
		jsonMesh["diffuseOrigin"] = mesh.diffuseOrigin;
		jsonMesh["normalmapOrigin"] = mesh.normalmapOrigin;
		jsonMesh["roughnessOrigin"] = mesh.roughnessOrigin;
		jsonMesh["metallicOrigin"] = mesh.metallicOrigin;

		jsonRoot["meshes"][meshIdx] = jsonMesh;
	}

	// Write to file
	std::ofstream jsonFile(filename.c_str());
	if (jsonFile.bad() || !jsonFile.is_open())
	{
		LOG_ERROR("Failed to save raw model file \"" << filename << "\"");
		return;
	}
	jsonFile << jsonRoot;
	jsonFile.close();

	// Write raw vertex buffer.
	std::ofstream rawBufferFile(rawbufferFilename, std::ios::binary);
	if (rawBufferFile.bad() || !rawBufferFile.is_open())
	{
		LOG_ERROR("Failed to save raw buffer file \"" << rawbufferFilename << "\"");
	}
	else
	{
		for (unsigned int i = 0; i < m_numVertices; ++i)
		{
			rawBufferFile.write(reinterpret_cast<const char*>(rawVertexData + i), sizeof(Vertex));
		}
		for (unsigned int i = 0; i < m_numTriangles * 3; ++i)
		{
			rawBufferFile.write(reinterpret_cast<const char*>(rawIndexData + i), 4);
		}
	}
	rawBufferFile.close();

	LOG_INFO("Wrote raw model file to \"" << filename << "\"");
}

std::shared_ptr<Model> Model::LoadFromRaw(const std::string& filename, const std::string& directory)
{
	std::ifstream jsonFile(filename);
	if (jsonFile.bad() || !jsonFile.is_open())
	{
		LOG_ERROR("Failed to load raw model file \"" << filename << "\"");
		return nullptr;
	}
	Json::Value root;
	jsonFile >> root;
	jsonFile.close();


	std::shared_ptr<Model> outModel(new Model(filename));

	Json::Value header = root["header"];
	if (header.get("version", -1).asInt() != m_rawModelVersion)
		LOG_WARNING("Raw model version does not match parser version!");

	std::string rawBufferFilename = PathUtils::AppendPath(directory, header.get("rawbufferFilename", "*").asString());
	if (rawBufferFilename.find('*') != std::string::npos)
	{
		LOG_ERROR("Raw model " << filename << " has no rawbuffer!");
		return nullptr;
	}
	outModel->m_numVertices = header.get("numVertices", 0).asUInt();
	outModel->m_numTriangles = header.get("numTriangles", 0).asUInt();

	// Bounding box
	Json::Value boundingBox = root["boundingBox"];
	outModel->m_boundingBox.min[0] = boundingBox["min"][0].asFloat();
	outModel->m_boundingBox.min[1] = boundingBox["min"][1].asFloat();
	outModel->m_boundingBox.min[2] = boundingBox["min"][2].asFloat();
	outModel->m_boundingBox.max[0] = boundingBox["max"][0].asFloat();
	outModel->m_boundingBox.max[1] = boundingBox["max"][1].asFloat();
	outModel->m_boundingBox.max[2] = boundingBox["max"][2].asFloat();

	// Meshes
	Json::Value defaultColor;
	defaultColor[0] = 1.0f;
	defaultColor[1] = 0.0f;
	defaultColor[2] = 1.0f;

	Json::Value meshes = root["meshes"];
	for (unsigned int meshIdx = 0; meshIdx < meshes.size(); ++meshIdx)
	{
		Json::Value jsonMesh = meshes[meshIdx];
		outModel->m_meshes.emplace_back();
		Mesh& mesh = outModel->m_meshes.back();

		mesh.startIndex = jsonMesh.get("startIndex", 0).asUInt();
		mesh.numIndices = jsonMesh.get("numIndices", 0).asUInt();
		mesh.diffuseOrigin = jsonMesh.get("diffuseOrigin", defaultColor);
		mesh.normalmapOrigin = jsonMesh.get("normalmapOrigin", "*default*");
		mesh.roughnessOrigin = jsonMesh.get("roughnessOrigin", TextureManager::GetInstance().s_defaultRoughness);
		mesh.metallicOrigin = jsonMesh.get("metallicOrigin", TextureManager::GetInstance().s_defaultMetallic);

		mesh.LoadTextures(directory);
	}


	// Load raw buffer.
	std::ifstream rawbufferFile(rawBufferFilename, std::ios::binary);
	if (rawbufferFile.bad() || !rawbufferFile.is_open())
	{
		LOG_ERROR("Failed to load raw model file \"" << rawBufferFilename << "\"");
		return nullptr;
	}

	unsigned int numBytes = outModel->m_numVertices * sizeof(Vertex);
	std::unique_ptr<char[]> vertexData(new char[numBytes]);
	rawbufferFile.read(vertexData.get(), numBytes);
	outModel->m_vertexBuffer.reset(new gl::Buffer(numBytes, gl::Buffer::UsageFlag::IMMUTABLE, vertexData.get()));
	vertexData.reset();

	numBytes = outModel->GetNumTriangles() * sizeof(std::uint32_t) * 3;
	std::unique_ptr<char[]> indexData(new char[numBytes]);
	rawbufferFile.read(indexData.get(), numBytes);
	outModel->m_indexBuffer.reset(new gl::Buffer(numBytes, gl::Buffer::UsageFlag::IMMUTABLE, indexData.get()));
	indexData.reset();

	return outModel;
}

std::shared_ptr<Model> Model::LoadViaAssimp(const std::string& filename, const std::string& directory, 
											std::unique_ptr<Vertex[]>& outVertices, std::unique_ptr<std::uint32_t[]>& outIndices)
{
	// Ignore line/point primitives
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

	const aiScene* scene = importer.ReadFile(filename,
		// Data validation and fixing
		aiProcess_ValidateDataStructure |
		//aiProcess_FixInfacingNormals	|
		aiProcess_FindInvalidData |

		// Data generation
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		//aiProcess_GenUVCoords |

		// Pretransform
		aiProcess_TransformUVCoords |
		aiProcess_ImproveCacheLocality |
		aiProcess_FlipUVs |

		// Remove redundant stuff
		aiProcess_JoinIdenticalVertices |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph |
		aiProcess_PreTransformVertices |
		aiProcess_RemoveRedundantMaterials
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
	outVertices.reset(new Vertex[output->m_numVertices]);
	Vertex* currentVertex = outVertices.get();
	for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		if(scene->mMeshes[i])
		{
			auto& mesh = *scene->mMeshes[i];
			for(unsigned int v = 0; v < mesh.mNumVertices; ++v)
			{
				memcpy(&currentVertex->position, &mesh.mVertices[v], sizeof(float) * 3);
				memcpy(&currentVertex->normal, &mesh.mNormals[v], sizeof(float) * 3);

				if (mesh.mBitangents && mesh.mTangents)
				{
					memcpy(&currentVertex->tangent, &mesh.mTangents[v], sizeof(float) * 3);

					// Retrieve bitangent handedness - see also http://www.terathon.com/code/tangent.html
					ei::Vec3 bitangent(mesh.mBitangents[v].x, mesh.mBitangents[v].y, mesh.mBitangents[v].z);
					currentVertex->tangent.w = (ei::dot(ei::cross(currentVertex->normal, ei::Vec3(currentVertex->tangent.x, currentVertex->tangent.x, currentVertex->tangent.x)), bitangent) < 0.0f) ? -1.0f : 1.0f;
				}
				else
				{
					ei::Vec3 tangent;
					if (fabs(currentVertex->normal.y) < 0.95)
						tangent = ei::normalize(ei::cross(ei::Vec3(0, 1, 0), currentVertex->normal));
					else
						tangent = ei::normalize(ei::cross(ei::Vec3(0, 0, 1), currentVertex->normal));
					currentVertex->tangent = ei::Vec4(tangent, 1.0);
				}
				
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
	output->m_vertexBuffer.reset(new gl::Buffer(sizeof(Vertex) * output->m_numVertices, gl::Buffer::UsageFlag::IMMUTABLE, outVertices.get()));

	// Load indices - Currently only 32bit indices
	outIndices.reset(new std::uint32_t[output->m_numTriangles * 3]);
	std::uint32_t indexOffset = 0;
	std::uint32_t* currentIndex = outIndices.get();
	for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		if(scene->mMeshes[i])
		{
			auto& mesh = *scene->mMeshes[i];

			output->m_meshes[i].numIndices = mesh.mNumFaces * 3;
			output->m_meshes[i].startIndex = static_cast<unsigned int>(currentIndex - outIndices.get());
			
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
	output->m_indexBuffer.reset(new gl::Buffer(sizeof(std::uint32_t) * output->m_numTriangles * 3, gl::Buffer::UsageFlag::IMMUTABLE, outIndices.get()));

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
						output->m_meshes[i].diffuseOrigin = diffuseTexture.C_Str();
					}
					else
					{
						aiColor3D aiColor = aiColor3D(0.0f, 0.0f, 0.0f);
						scene->mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor);
						ei::Vec3 diffuseColor(aiColor.r, aiColor.g, aiColor.b);
						output->m_meshes[i].diffuseOrigin[0] = diffuseColor.r;
						output->m_meshes[i].diffuseOrigin[1] = diffuseColor.g;
						output->m_meshes[i].diffuseOrigin[2] = diffuseColor.b;
					}
				}

				{
					aiString normalTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_NORMALS, 0, &normalTexture);
					if (normalTexture.length)
						output->m_meshes[i].normalmapOrigin = normalTexture.C_Str();
					else
						output->m_meshes[i].normalmapOrigin = "*default*";
				}
				{
					aiString metallicTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_SPECULAR, 0, &metallicTexture);
					if (metallicTexture.length)
						output->m_meshes[i].metallicOrigin = metallicTexture.C_Str();
					else
						output->m_meshes[i].metallicOrigin = TextureManager::GetInstance().s_defaultMetallic;
				}
				{
					aiString roughnessTexture;
					scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_SHININESS, 0, &roughnessTexture);
					if (roughnessTexture.length)
						output->m_meshes[i].roughnessOrigin = roughnessTexture.C_Str();
					else
						output->m_meshes[i].roughnessOrigin = TextureManager::GetInstance().s_defaultRoughness;
				}

				output->m_meshes[i].LoadTextures(directory);
			}
		}
	}

	importer.FreeScene();

	return output;
}

void Model::Mesh::LoadTextures(const std::string& directory)
{
	if (diffuseOrigin.isArray())
		diffuse = TextureManager::GetInstance().GetDiffuse(ei::Vec3(diffuseOrigin[0].asFloat(), diffuseOrigin[1].asFloat(), diffuseOrigin[2].asFloat()));
	else
		diffuse = TextureManager::GetInstance().GetDiffuse(PathUtils::AppendPath(directory, diffuseOrigin.asString()));

	std::string normalmapSpecifier = normalmapOrigin.asString();
	if (normalmapSpecifier.find('*') != std::string::npos || normalmapSpecifier.empty())
		normalmap = TextureManager::GetInstance().GetDefaultNormal();
	else
		normalmap = TextureManager::GetInstance().GetNormalmap(PathUtils::AppendPath(directory, normalmapSpecifier));

	if (roughnessOrigin.isDouble() && metallicOrigin.isDouble())
		roughnessMetallic = TextureManager::GetInstance().GetRoughnessMetallic(roughnessOrigin.asFloat(), metallicOrigin.asFloat());
	else if (roughnessOrigin.isDouble())
		roughnessMetallic = TextureManager::GetInstance().GetRoughnessMetallic(roughnessOrigin.asFloat(), PathUtils::AppendPath(directory, metallicOrigin.asString()));
	else if (metallicOrigin.isDouble())
		roughnessMetallic = TextureManager::GetInstance().GetRoughnessMetallic(PathUtils::AppendPath(directory, roughnessOrigin.asString()), metallicOrigin.asFloat());
	else
		roughnessMetallic = TextureManager::GetInstance().GetRoughnessMetallic(PathUtils::AppendPath(directory, roughnessOrigin.asString()), PathUtils::AppendPath(directory, metallicOrigin.asString()));

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