#include "texturemanager.hpp"

#include <glhelper/texture2D.hpp>
#include "../utilities/logger.hpp"

#include <stb_image.h>

const float TextureManager::s_defaultRoughness = 0.2f;
const float TextureManager::s_defaultMetallic = 0.0f;

TextureManager& TextureManager::GetInstance()
{
	static TextureManager instance;
	return instance;
}

TextureManager::TextureManager()
{
	ei::Vec3 defaultNormal(0.5f, 0.5f, 1.0f);
	m_defaultNormalmap = std::make_shared<gl::Texture2D>(1, 1, gl::TextureFormat::RGB8, reinterpret_cast<const char*>(&defaultNormal), gl::TextureSetDataFormat::RGB, gl::TextureSetDataType::FLOAT);

	ei::Vec2 defaultRoughnessMetallic(s_defaultRoughness, s_defaultMetallic);
	m_defaultRoughnessMetallic = std::make_shared<gl::Texture2D>(1, 1, gl::TextureFormat::RG8, reinterpret_cast<const char*>(&defaultRoughnessMetallic), gl::TextureSetDataFormat::RG, gl::TextureSetDataType::FLOAT);
}

TextureManager::~TextureManager()
{
}

std::shared_ptr<gl::Texture2D> TextureManager::GetDiffuse(const std::string& filename)
{
	auto textureEntry = m_diffuseTextures.find(filename);
	if (textureEntry == m_diffuseTextures.end())
	{
		std::shared_ptr<gl::Texture2D> newTexture(std::move(gl::Texture2D::LoadFromFile(filename, true, true)));
		if (newTexture)
		{
			m_diffuseTextures.insert(std::make_pair(filename, newTexture));
			LOG_INFO("Loaded diffuse texture \"" << filename << "\" " << std::to_string(newTexture->GetWidth()) << "x" << std::to_string(newTexture->GetHeight()));
		}
		else
		{
			LOG_ERROR("Failed to load diffuse texture \"" << filename << "\"");
		}
		return newTexture;
	}
	return textureEntry->second;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetDiffuse(const ei::Vec3& color)
{
	std::string name = "§§color: " + std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b) + " §§";
	auto textureEntry = m_diffuseTextures.find(name);
	if (textureEntry == m_diffuseTextures.end())
	{

		std::shared_ptr<gl::Texture2D> newTexture(new gl::Texture2D(1, 1, gl::TextureFormat::SRGB8, reinterpret_cast<const char*>(&color), gl::TextureSetDataFormat::RGB, gl::TextureSetDataType::FLOAT));
		if (newTexture)
		{
			m_diffuseTextures.insert(std::make_pair(name, newTexture));
			LOG_INFO("Loaded single colored diffuse texture \"" << std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b));
		}
		else
		{
			LOG_ERROR("Failed to create single colored diffuse texture.");
		}
		return newTexture;
	}
	return textureEntry->second;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetNormalmap(const std::string& filename)
{
	auto textureEntry = m_normalmapTextures.find(filename);
	if (textureEntry == m_normalmapTextures.end())
	{
		std::shared_ptr<gl::Texture2D> newTexture(std::move(gl::Texture2D::LoadFromFile(filename, true, false)));
		if (newTexture)
		{
			m_normalmapTextures.insert(std::make_pair(filename, newTexture));
			LOG_INFO("Loaded normalmap \"" << filename << "\" " << std::to_string(newTexture->GetWidth()) << "x" << std::to_string(newTexture->GetHeight()));
		}
		else
		{
			LOG_ERROR("Failed to load normalmap \"" << filename << "\"");
		}
		return newTexture;
	}
	return textureEntry->second;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetRoughnessMetallic(const std::string& roughnessTexture, const std::string& metallicTexture)
{
	std::string identifier = roughnessTexture + "_" + metallicTexture;
	auto textureEntry = m_roughnessMetallicTextures.find(identifier);
	if (textureEntry != m_roughnessMetallicTextures.end())
		return textureEntry->second;

	int roughnessTexSizeX = -1;
	int roughnessTexSizeY = -1;
	int roughnessNumComps = -1;
	stbi_uc* roughnessData = stbi_load(roughnessTexture.c_str(), &roughnessTexSizeX, &roughnessTexSizeY, &roughnessNumComps, 3);
	if (!roughnessData)
	{
		LOG_ERROR("Error loading roughness texture \"" << roughnessTexture << "\": " << stbi_failure_reason());
		stbi_image_free(roughnessData);
		return nullptr;
	}

	std::unique_ptr<unsigned char[]> textureData(new unsigned char[roughnessTexSizeX * roughnessTexSizeY * 2]);
	if (!metallicTexture.empty())
	{
		int metallicTexSizeX = -1;
		int metallicTexSizeY = -1;
		int metallicNumComps = -1;
		stbi_uc* metallicData = stbi_load(metallicTexture.c_str(), &metallicTexSizeX, &metallicTexSizeY, &metallicNumComps, 3);
		if (!metallicData)
		{
			LOG_ERROR("Error loading metallic texture \"" << metallicTexture << "\". " << stbi_failure_reason());
			stbi_image_free(metallicData);
			stbi_image_free(roughnessData);
			return nullptr;
		}
		if (metallicTexSizeX != roughnessTexSizeX || metallicTexSizeY != roughnessTexSizeY)
		{
			LOG_ERROR("Size of metallic texture does not match size of roughness texture!");
			stbi_image_free(metallicData);
			stbi_image_free(roughnessData);
			return nullptr;
		}

		for (int i = 0; i < metallicTexSizeX * metallicTexSizeY; ++i)
		{
			textureData[i * 2] = roughnessData[i*3];
			textureData[i * 2 + 1] = metallicData[i*3];
		}
		stbi_image_free(metallicData);
	}
	else
	{
		// Use default metallic value.
		unsigned char defaultMetallicChar = static_cast<unsigned char>(s_defaultMetallic * 255);
		for (int i = 0; i < roughnessTexSizeX * roughnessTexSizeY; ++i)
		{
			textureData[i * 2] = roughnessData[i*3];
			textureData[i * 2 + 1] = defaultMetallicChar;
		}
	}
	stbi_image_free(roughnessData);


	auto texture = std::make_shared<gl::Texture2D>(roughnessTexSizeX, roughnessTexSizeY, gl::TextureFormat::RG8, textureData.get(), gl::TextureSetDataFormat::RG, gl::TextureSetDataType::UNSIGNED_BYTE);
	m_roughnessMetallicTextures.insert(std::make_pair(identifier, texture));

	return texture;
}