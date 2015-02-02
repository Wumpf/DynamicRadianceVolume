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
/// \todo Texture hashing should be sensible to ALL parameters.
class TextureManager
{
public:
	static TextureManager& GetInstance();

	/// \param srgb			If true the file is assumed to be in srgb. Has no affect if the texture was already loaded!
	/// \param genMipMaps	If true mipmaps will be generated. Has no affect if the texture was already loaded!
	/// \attention Only filename will be used for lookup!
	/// \param nullptr if texture loading failed.
	std::shared_ptr<gl::Texture2D> GetTexture(const std::string& filename, bool genMipMaps, bool srgb);

	/// Creates 1x1 pixel from color.
	std::shared_ptr<gl::Texture2D> GetTexture(const ei::Vec3& color, bool srgb);

private:
	TextureManager();
	~TextureManager();

	std::unordered_map<std::string, std::shared_ptr<gl::Texture2D>> m_textures;
};