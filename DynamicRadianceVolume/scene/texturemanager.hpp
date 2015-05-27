#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <ei/vector.hpp>

namespace gl
{
	class Texture2D;
}


/// Super easy texture manager to avoid loading a texture twice.
///
/// Distinguishes different texture usages and thus implicitly is responsible for format layout.
/// It is meant to be used by the Model  class
/// \see Model, Model::Mesh
class TextureManager
{
public:
	static TextureManager& GetInstance();

	/// Gets diffuse/basecolor from file.
	/// Interprets data as srgb and generates mipmaps.
	std::shared_ptr<gl::Texture2D> GetDiffuse(const std::string& filename);

	/// Generates diffuse/basecolor from color.
	std::shared_ptr<gl::Texture2D> GetDiffuse(const ei::Vec3& color);


	/// Gets normalmap from file.
	/// Interprets data as non-srgb and generates mipmaps.
	std::shared_ptr<gl::Texture2D> GetNormalmap(const std::string& filename);

	/// Gets default normalmap with up-pointing normal.
	std::shared_ptr<gl::Texture2D> GetDefaultNormal() { return m_defaultNormalmap; }



	std::shared_ptr<gl::Texture2D> GetRoughnessMetallic(const std::string& roughnessTexture, const std::string& metallicTexture);

	std::shared_ptr<gl::Texture2D> GetRoughnessMetallic(const std::string& roughnessTexture) { GetRoughnessMetallic(roughnessTexture, ""); }

	std::shared_ptr<gl::Texture2D> GetDefaultRoughnessMetallic() { return m_defaultRoughnessMetallic; }


private:
	TextureManager();
	~TextureManager();

	static const float s_defaultRoughness;
	static const float s_defaultMetallic;

	std::shared_ptr<gl::Texture2D> m_defaultNormalmap;
	std::shared_ptr<gl::Texture2D> m_defaultRoughnessMetallic;
	std::unordered_map<std::string, std::shared_ptr<gl::Texture2D>> m_diffuseTextures;
	std::unordered_map<std::string, std::shared_ptr<gl::Texture2D>> m_normalmapTextures;
	std::unordered_map<std::string, std::shared_ptr<gl::Texture2D>> m_roughnessMetallicTextures;
};