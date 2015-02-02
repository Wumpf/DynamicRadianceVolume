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

std::shared_ptr<gl::Texture2D> TextureManager::GetTexture(const ei::Vec3& color, bool srgb)
{
	std::string name = "§§color: " + std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b) + " §§";
	auto textureEntry = m_textures.find(name);
	if (textureEntry == m_textures.end())
	{
		
		std::shared_ptr<gl::Texture2D> newTexture(new gl::Texture2D(1, 1, srgb ? gl::TextureFormat::SRGB8 : gl::TextureFormat::RGB8, 
																	reinterpret_cast<const char*>(&color), gl::TextureSetDataFormat::RGB, gl::TextureSetDataType::FLOAT));
		if (newTexture)
		{
			m_textures.insert(std::make_pair(name, newTexture));
			LOG_INFO("Loaded single colored texture \"" << std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b));
		}
		else
		{
			LOG_ERROR("Failed to create single colored texture.");
		}
		return newTexture;
	}
	return textureEntry->second;
}