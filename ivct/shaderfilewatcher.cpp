#include "shaderfilewatcher.hpp"
#include "utilities/pathutils.hpp"
#include <glhelper/shaderobject.hpp>


ShaderFileWatcher::ShaderFileWatcher() :
	m_watchID(std::numeric_limits<FW::WatchID>::max())
{
}

ShaderFileWatcher& ShaderFileWatcher::Instance()
{
	static ShaderFileWatcher instance;
	return instance;
}

void ShaderFileWatcher::SetShaderWatchDirectory(const std::string& _watchDir)
{
	m_shaderFileWatcher.removeWatch(m_watchID);
	m_shaderFileWatcher.addWatch(_watchDir, this);
}

void ShaderFileWatcher::RegisterShaderForReloadOnChange(gl::ShaderObject* _shader)
{
	m_registeredShader.insert(_shader);
}

void ShaderFileWatcher::UnregisterShaderForReloadOnChange(gl::ShaderObject* _shader)
{
	auto it = m_registeredShader.find(_shader);
	if (it != m_registeredShader.end())
		m_registeredShader.erase(it);
}

void ShaderFileWatcher::handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, FW::Action action)
{
	if (action == FW::Actions::Modified)
	{
		std::string fullFilename = PathUtils::AppendPath(dir, filename);
		for (auto shader : m_registeredShader)
		{
			shader->ShaderFileChangeHandler(fullFilename);
		}
	}
}

void ShaderFileWatcher::Update()
{
	m_shaderFileWatcher.update();
}