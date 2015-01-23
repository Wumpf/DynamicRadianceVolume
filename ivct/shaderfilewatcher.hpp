#pragma once

#include <unordered_set>
#include <memory>
#include <string>

#include "FileWatcher/FileWatcher.h"

namespace gl
{
	class ShaderObject;
}

class ShaderFileWatcher : public FW::FileWatchListener
{
public:
	static ShaderFileWatcher& Instance();

	void SetShaderWatchDirectory(const FW::String& _watchDir);

	void RegisterShaderForReloadOnChange(gl::ShaderObject* _shader);
	void UnregisterShaderForReloadOnChange(gl::ShaderObject* _shader);

	void Update();

private:
	void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, FW::Action action);

	ShaderFileWatcher();
	~ShaderFileWatcher() {}

	FW::FileWatcher m_shaderFileWatcher;
	FW::WatchID m_watchID;

	std::unordered_set<gl::ShaderObject*> m_registeredShader;
};