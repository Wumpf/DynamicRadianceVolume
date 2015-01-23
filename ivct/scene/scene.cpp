#include "scene.hpp"
#include "model.hpp"
#include "camera/camera.hpp"

#include <glhelper/samplerobject.hpp>
#include <glhelper/shaderobject.hpp>
#include <glhelper/texture3d.hpp>
#include <glhelper/screenalignedtriangle.hpp>


Scene::Scene() :
	m_boundingBoxMin(std::numeric_limits<float>::max()),
	m_boundingBoxMax(std::numeric_limits<float>::min()),
	m_samplerLinearMipNearest(gl::SamplerObject::GetSamplerObject(gl::SamplerObject::Desc(gl::SamplerObject::Filter::LINEAR, gl::SamplerObject::Filter::LINEAR, 
																	gl::SamplerObject::Filter::NEAREST, gl::SamplerObject::Border::CLAMP)))
{
	Model::CreateVAO();

	m_screenTriangle = std::make_unique<gl::ScreenAlignedTriangle>();

	m_simpleShader = std::make_unique<gl::ShaderObject>("simple draw");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/simple.vert");
	m_simpleShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/simple.frag");
	m_simpleShader->CreateProgram();

	m_voxelizationShader = std::make_unique<gl::ShaderObject>("voxelization");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/voxelize.vert");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::GEOMETRY, "shader/voxelize.geom");
	m_voxelizationShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxelize.frag");
	m_voxelizationShader->CreateProgram();

	m_voxelDebugShader = std::make_unique<gl::ShaderObject>("voxel debug");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::VERTEX, "shader/screenTri.vert");
	m_voxelDebugShader->AddShaderFromFile(gl::ShaderObject::ShaderType::FRAGMENT, "shader/voxeldebug.frag");
	m_voxelDebugShader->CreateProgram();

	m_constantUniformBuffer.Init(*m_simpleShader, "Constant");
	m_constantUniformBuffer.BindBuffer(0);

	m_perFrameUniformBuffer.Init(*m_simpleShader, "PerFrame");
	m_perFrameUniformBuffer.BindBuffer(1);

	// size for testing only. R8 sounds also like a bad idea since quite everything works in (at least) 32bytes on gpu
	m_voxelSceneTexture = std::make_unique<gl::Texture3D>(512, 256, 256, gl::TextureFormat::R8, 1);
}

Scene::~Scene()
{
	Model::DestroyVAO();
}

void Scene::UpdateConstantUBO()
{
	ei::Vec3 voxelVolumeWorldMin(m_boundingBoxMin);
	ei::Vec3 voxelVolumeWorldMax(m_boundingBoxMax);
	ei::Vec3 voxelVolumeSizePix(static_cast<float>(m_voxelSceneTexture->GetWidth()), static_cast<float>(m_voxelSceneTexture->GetHeight()), static_cast<float>(m_voxelSceneTexture->GetDepth()));

	m_constantUniformBuffer.GetBuffer()->Map();
	m_constantUniformBuffer["VoxelVolumeWorldMin"].Set(voxelVolumeWorldMin);
	m_constantUniformBuffer["VoxelVolumeWorldMax"].Set(voxelVolumeWorldMax);
	m_constantUniformBuffer["VoxelSizeInWorld"].Set((voxelVolumeWorldMax - voxelVolumeWorldMin) / voxelVolumeSizePix);
	m_constantUniformBuffer.GetBuffer()->Unmap();
}

void Scene::UpdatePerFrameUBO(Camera& camera)
{
	auto viewProjection = camera.ComputeViewProjection();

	m_perFrameUniformBuffer.GetBuffer()->Map();
	m_perFrameUniformBuffer["ViewProjection"].Set(viewProjection);
	m_perFrameUniformBuffer["InverseViewProjection"].Set(ei::invert(viewProjection));
	m_perFrameUniformBuffer["CameraPosition"].Set(camera.GetPosition());
	m_perFrameUniformBuffer.GetBuffer()->Unmap();
}

void Scene::AddModel(const std::string& filename)
{
	LOG_INFO("Loading " << filename << " ...");
	std::shared_ptr<Model> model = Model::FromFile(filename);
	if (model)
	{
		m_models.push_back(model);
		m_boundingBoxMin = ei::min(m_boundingBoxMin, model->GetBoundingBoxMin());
		m_boundingBoxMax = ei::max(m_boundingBoxMin, model->GetBoundingBoxMax());
	}

	// Todo: Do this only when scene is "finished"
	UpdateConstantUBO();
}

void Scene::Draw(Camera& camera)
{
	UpdatePerFrameUBO(camera);

	VoxelizeScene();
	DrawVoxelRepresentation();

	//m_simpleShader->Activate();
	//DrawScene();
}

void Scene::DrawScene()
{
	Model::BindVAO();

	for(const std::shared_ptr<Model>& model : m_models)
	{
		model->BindBuffers();
		for(const Model::Mesh& mesh : model->GetMeshes())
		{
			if(mesh.diffuseTexture)
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

	m_samplerLinearMipNearest.BindSampler(0);
	m_voxelSceneTexture->Bind(0);
	m_voxelDebugShader->Activate();
	
	m_screenTriangle->Draw();

	// Reenable depth buffer.
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);
}

void Scene::VoxelizeScene()
{
	// Disable depthbuffering.
	GL_CALL(glDisable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_FALSE);

	// Disable color write
	GL_CALL(glColorMask, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Viewport in size of voxel volume.
	GL_CALL(glViewport, 0, 0, m_voxelSceneTexture->GetWidth(), m_voxelSceneTexture->GetHeight());

	// Clear volume.
	m_voxelSceneTexture->ClearToZero();

	// Draw
	m_voxelSceneTexture->BindImage(0, gl::Texture::ImageAccess::WRITE);
	m_voxelizationShader->Activate();
	DrawScene();

	// Reset viewport
	GL_CALL(glViewport, 0, 0, 1024, 768); // TODO

	// Reenable depth buffer.
	GL_CALL(glEnable, GL_DEPTH_TEST);
	GL_CALL(glDepthMask, GL_TRUE);

	// Reenable color write
	GL_CALL(glColorMask, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}