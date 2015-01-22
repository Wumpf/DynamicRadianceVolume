#pragma once

#include <memory>
#include <vector>
#include <string>

// Directly included for convenience: Not having uniform buffer as pointer enables nicer [] syntax for vars.
#include <glhelper/uniformbuffer.hpp>

namespace gl
{
	class ShaderObject;
	class Texture3D;
	class ScreenAlignedTriangle;
	class SamplerObject;
}
class Model;
class Camera;

class Scene
{
public:
	Scene();
	~Scene();

	void AddModel(const std::string& filename);

	void Draw(Camera& camera);

private:
	void UpdateConstantUBO();
	void UpdatePerFrameUBO(Camera& camera);

	void DrawScene();
	void DrawVoxelRepresentation();
	void VoxelizeScene();


	std::unique_ptr<gl::ScreenAlignedTriangle> m_screenTriangle;

	std::unique_ptr<gl::ShaderObject> m_simpleShader;
	std::unique_ptr<gl::ShaderObject> m_voxelizationShader;
	std::unique_ptr<gl::ShaderObject> m_voxelDebugShader;

	gl::UniformBufferView m_constantUniformBuffer;
	gl::UniformBufferView m_perFrameUniformBuffer;

	std::unique_ptr<gl::Texture3D> m_voxelSceneTexture;

	std::vector<std::shared_ptr<Model>> m_models;
	ei::Vec3 m_boundingBoxMin;
	ei::Vec3 m_boundingBoxMax;

	const gl::SamplerObject& m_samplerLinearMipNearest;
};

