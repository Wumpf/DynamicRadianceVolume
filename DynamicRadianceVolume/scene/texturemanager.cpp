#include "texturemanager.hpp"

#include <glhelper/texture2D.hpp>
#include "../utilities/logger.hpp"

#include <stb_image.h>
#include "utilities/utils.hpp"

const float TextureManager::s_defaultRoughness = 0.4f;
const float TextureManager::s_defaultMetallic = 0.001f;

TextureManager& TextureManager::GetInstance()
{
	static TextureManager instance;
	return instance;
}

TextureManager::TextureManager()
{
	ei::Vec3 defaultNormal(0.5f, 0.5f, 1.0f);
	m_defaultNormalmap = std::make_shared<gl::Texture2D>(1, 1, gl::TextureFormat::RGB8, reinterpret_cast<const char*>(&defaultNormal), gl::TextureSetDataFormat::RGB, gl::TextureSetDataType::FLOAT);
}

TextureManager::~TextureManager()
{
}

TextureManager::Channel TextureManager::ChannelFromChar(char c)
{
	switch (c)
	{
	case 'A':
	case 'a':
		return Channel::A;

	case 'G':
	case 'g':
		return Channel::G;

	case 'B':
	case 'b':
		return Channel::B;

	default:
		return Channel::R;
	}
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


stbi_uc* LoadTexture(const std::string filename, int& sizeX, int& sizeY)
{
	sizeX = -1;
	sizeY = -1;
	int roughnessNumComps = -1;
	stbi_uc* data = stbi_load(filename.c_str(), &sizeX, &sizeY, &roughnessNumComps, 4);
	if (!data)
	{
		LOG_ERROR("Error loading texture \"" << filename << "\": " << stbi_failure_reason());
		stbi_image_free(data);
		return nullptr;
	}
	return data;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetRoughnessMetallic(const std::string& roughnessTexture, const std::string& metallicTexture,
																bool invertRoughnessTexture, Channel roughnessTextureChannel, Channel metallicTextureChannel)
{
	std::string identifier = roughnessTexture + "_" + metallicTexture;
	auto textureEntry = m_roughnessMetallicTextures.find(identifier);
	if (textureEntry != m_roughnessMetallicTextures.end())
		return textureEntry->second;

	int roughnessTexSizeX, roughnessTexSizeY;
	stbi_uc* roughnessData = LoadTexture(roughnessTexture, roughnessTexSizeX, roughnessTexSizeY);
	if (!roughnessData) return nullptr;

	stbi_uc* metallicData = nullptr;
	int metallicTexSizeX = roughnessTexSizeX;
	int metallicTexSizeY = roughnessTexSizeY;
	if (roughnessTexture != metallicTexture)
	{
		metallicData = LoadTexture(metallicTexture, metallicTexSizeX, metallicTexSizeY);
		if (!metallicData)
		{
			stbi_image_free(roughnessData);
			return nullptr;
		}
	}
	else
		metallicData = roughnessData;

	if (metallicTexSizeX != roughnessTexSizeX || metallicTexSizeY != roughnessTexSizeY)
	{
		LOG_ERROR("Size of metallic texture does not match size of roughness texture!");
		if (roughnessData != metallicData)
			stbi_image_free(metallicData);
		stbi_image_free(roughnessData);
		return nullptr;
	}

	std::unique_ptr<unsigned char[]> textureData(new unsigned char[roughnessTexSizeX * roughnessTexSizeY * 2]);
	if (invertRoughnessTexture)
	{
		for (int i = 0; i < metallicTexSizeX * metallicTexSizeY; ++i)
		{
			textureData[i * 2] = 255 - roughnessData[i * 4 + (int)roughnessTextureChannel];
			textureData[i * 2 + 1] = metallicData[i * 4 + (int)metallicTextureChannel];
		}
	}
	else
	{
		for (int i = 0; i < metallicTexSizeX * metallicTexSizeY; ++i)
		{
			textureData[i * 2] = roughnessData[i * 4 + (int)roughnessTextureChannel];
			textureData[i * 2 + 1] = metallicData[i * 4 + (int)metallicTextureChannel];
		}
	}
	if (roughnessData != metallicData)
		stbi_image_free(metallicData);
	stbi_image_free(roughnessData);

	auto texture = std::make_shared<gl::Texture2D>(roughnessTexSizeX, roughnessTexSizeY, gl::TextureFormat::RG8, textureData.get(), gl::TextureSetDataFormat::RG, gl::TextureSetDataType::UNSIGNED_BYTE);
	m_roughnessMetallicTextures.insert(std::make_pair(identifier, texture));

	LOG_INFO("Loaded roughness/metallic maps \"" << roughnessTexture << "\" - \"" << metallicTexture << "\"");

	return texture;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetRoughnessMetallic(const std::string& roughnessTexture, float metallicValue)
{
	std::string identifier = roughnessTexture + "_§§" + std::to_string(metallicValue);
	auto textureEntry = m_roughnessMetallicTextures.find(identifier);
	if (textureEntry != m_roughnessMetallicTextures.end())
		return textureEntry->second;

	int roughnessTexSizeX, roughnessTexSizeY;
	stbi_uc* roughnessData = LoadTexture(roughnessTexture, roughnessTexSizeX, roughnessTexSizeY);
	if (!roughnessData) return nullptr;

	std::unique_ptr<unsigned char[]> textureData(new unsigned char[roughnessTexSizeX * roughnessTexSizeY * 2]);
	unsigned char metallicChar = static_cast<unsigned char>(Clamp(metallicValue, 0.0f, 1.0f) * 255);
	for (int i = 0; i < roughnessTexSizeX * roughnessTexSizeY; ++i)
	{
		textureData[i * 2] = roughnessData[i*4];
		textureData[i * 2 + 1] = metallicChar;
	}
	stbi_image_free(roughnessData);


	auto texture = std::make_shared<gl::Texture2D>(roughnessTexSizeX, roughnessTexSizeY, gl::TextureFormat::RG8, textureData.get(), gl::TextureSetDataFormat::RG, gl::TextureSetDataType::UNSIGNED_BYTE);
	m_roughnessMetallicTextures.insert(std::make_pair(identifier, texture));

	LOG_INFO("Loaded roughness maps \"" << roughnessTexture << "\" - fixed metallic: " << metallicValue << "\"");

	return texture;
}

std::shared_ptr<gl::Texture2D> TextureManager::GetRoughnessMetallic(float roughnessValue, const std::string& metallicTexture)
{
	std::string identifier = "§§_" + std::to_string(roughnessValue) + " " + metallicTexture;
	auto textureEntry = m_roughnessMetallicTextures.find(identifier);
	if (textureEntry != m_roughnessMetallicTextures.end())
		return textureEntry->second;

	int metallicTexSizeX, metallicTexSizeY;
	stbi_uc* metallicData = LoadTexture(metallicTexture, metallicTexSizeX, metallicTexSizeY);
	if (!metallicData) return nullptr;

	std::unique_ptr<unsigned char[]> textureData(new unsigned char[metallicTexSizeX * metallicTexSizeY * 2]);
	unsigned char roughnessChar = static_cast<unsigned char>(Clamp(roughnessValue, 0.0f, 1.0f) * 255);
	for (int i = 0; i < metallicTexSizeX * metallicTexSizeY; ++i)
	{
		textureData[i * 2] = roughnessChar;
		textureData[i * 2 + 1] = metallicData[i*4];
	}
	stbi_image_free(metallicData);


	auto texture = std::make_shared<gl::Texture2D>(metallicTexSizeX, metallicTexSizeY, gl::TextureFormat::RG8, textureData.get(), gl::TextureSetDataFormat::RG, gl::TextureSetDataType::UNSIGNED_BYTE);
	m_roughnessMetallicTextures.insert(std::make_pair(identifier, texture));

	LOG_INFO("Loaded roughness maps \"" << "\" - fixed roughness: " << roughnessValue << "\" - " << metallicTexture);

	return texture;
}


std::shared_ptr<gl::Texture2D> TextureManager::GetRoughnessMetallic(float roughnessValue, float metallicValue)
{
	roughnessValue = Clamp(roughnessValue, 0.0f, 1.0f);
	metallicValue = Clamp(metallicValue, 0.0f, 1.0f);

	std::string name = "§§roughness: " + std::to_string(roughnessValue) + " metallic: " + std::to_string(metallicValue);
	auto textureEntry = m_roughnessMetallicTextures.find(name);
	if (textureEntry == m_roughnessMetallicTextures.end())
	{
		unsigned char values[2];
		values[0] = static_cast<unsigned char>(roughnessValue * 255);
		values[1] = static_cast<unsigned char>(metallicValue * 255);
		std::shared_ptr<gl::Texture2D> newTexture(new gl::Texture2D(1, 1, gl::TextureFormat::RG8, values, gl::TextureSetDataFormat::RG, gl::TextureSetDataType::UNSIGNED_BYTE));
		if (newTexture)
		{
			m_roughnessMetallicTextures.insert(std::make_pair(name, newTexture));
			LOG_INFO("Loaded single value roughness/metallic texture \"" << std::to_string(roughnessValue) + "/" + std::to_string(metallicValue));
		}
		else
		{
			LOG_ERROR("Failed to create roughness/metallic texture from values.");
		}
		
		return newTexture;
	}

	return textureEntry->second;
}