#include "scene.hpp"
#include "model.hpp"
#include "camera/camera.hpp"

#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/screenalignedtriangle.hpp>


Scene::Scene()
{
	Model::CreateVAO();

	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_simpleShader = std::make_unique<gl::ShaderObject>("simple draw");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/simple.vert");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/simple.frag");
	m_simpleShader->CreateProgram();

	m_voxelDebugShader = std::make_unique<gl::ShaderObject>("voxel debug");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_voxelDebugShader->CreateProgram();

	m_constantUniformBuffer.Init(*m_simpleShader, "Constant");
	m_constantUniformBuffer.BindBuffer(0);

	m_perFrameUniformBuffer.Init(*m_simpleShader, "PerFrame");
	m_perFrameUniformBuffer.BindBuffer(1);

	// size for testing only. R8 sounds also like a bad idea since quite everything works in (at least) 32bytes on gpu
	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(128, 128, 128, gl::TextureFormat::R8, 1);

	unsigned int voxelDataSize = m_voxelSceneTexture->GetWidth() * m_voxelSceneTexture->GetHeight() * m_voxelSceneTexture->GetDepth();
	std::unique_ptr<std::uint8_t[]> voxelData(new std::uint8_t[voxelDataSize]);
	for (unsigned int i = 0; i < voxelDataSize; ++i)
		voxelData[i] = i % 255;
	m_voxelSceneTexture->SetData(0, gl::TextureSetDataFormat::RED, gl::TextureSetDataType::UNSIGNED_BYTE, voxelData.get());
}

Scene::~Scene()
{
	Model::DestroyVAO();
}

void Scene::AddModel(const std::string& filename)
{
	LOG_INFO("Loading " << filename << " ...");
	std::shared_ptr<Model> model = Model::FromFile(filename);
	if(model)
		models.push_back(model);
}

void Scene::Draw(Camera& camera)
{
	auto viewProjection = camera.ComputeViewProjection();

	m_perFrameUniformBuffer.GetBuffer()->Map();
	m_perFrameUniformBuffer["ViewProjection"].Set(viewProjection);
	m_perFrameUniformBuffer["InverseViewProjection"].Set(ei::invert(viewProjection));
	m_perFrameUniformBuffer["CameraPosition"].Set(camera.GetPosition());
	m_perFrameUniformBuffer.GetBuffer()->Unmap();

	DrawVoxelRepresentation();
}

void Scene::DrawScene()
{
	m_simpleShader->Activate();
	Model::BindVAO();

	for (const std::shared_ptr<Model>& model : models)
	{
		model->BindBuffers();
		for (const Model::Mesh& mesh : model->GetMeshes())
		{
			if (mesh.diffuseTexture)
				mesh.diffuseTexture->Bind(0);
			GL_CALL(glDrawElements, GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(sizeof(std::uint32_t) * mesh.startIndex));
		}
	}
}

void Scene::DrawVoxelRepresentation()
{
	// Disable depthbuffering.
	GL_CALL(glDisable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	m_voxelSceneTexture->Bind(0);
	m_voxelDebugShader->Activate();
	
	m_screenTriangle->Draw();

	// Reenable depth buffer.
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
}