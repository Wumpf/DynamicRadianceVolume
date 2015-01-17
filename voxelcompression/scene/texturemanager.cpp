#include "texturemanager.hpp"

#include <glhelper/texture2D.hpp>
#include "../utilities/logger.hpp"


TextureManager& TextureManager::GetInstance()
{
	static TextureManager instance;
	return instance;
}

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{
}

std::shared_ptr<gl::Texture2D> TextureManager::GetTexture(const std::string& filename, bool genMipMaps, bool srgb)
{
	auto textureEntry = m_textures.find(filename);
	if (textureEntry == m_textures.end())
	{
		std::shared_ptr<gl::Texture2D> newTexture(std::move(gl::Texture2D::LoadFromFile(filename, genMipMaps, srgb)));
		if (newTexture)
		{
			m_textures.insert(std::make_pair(filename, newTexture));
			LOG_INFO("Loaded texture \"" << filename << "\" " << std::to_string(newTexture->GetWidth()) << "x" << std::to_string(newTexture->GetHeight()));
		}
		else
		{
			LOG_ERROR("Failed to load texture \"" << filename << "\"");
		}
		return newTexture;
	}
	return textureEntry->second;
}