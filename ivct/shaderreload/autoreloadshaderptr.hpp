#pragma once

namespace gl
{
	class ShaderObject;
}

/// A simplified unique ptr that registers to the shaderfilewatcher on creation and deregisters on destruction.
class AutoReloadShaderPtr
{
public:
	AutoReloadShaderPtr(AutoReloadShaderPtr& shader) = delete;

	AutoReloadShaderPtr() : m_shader(nullptr) {}
	/// Registers via ShaderFileWatcher::RegisterShaderForReloadOnChange
	AutoReloadShaderPtr(gl::ShaderObject* shader);
	/// Unregisters via ShaderFileWatcher::UnregisterShaderForReloadOnChange and destroys using delete.
	~AutoReloadShaderPtr();
	
	gl::ShaderObject* get() const			{ return m_shader; }
	gl::ShaderObject* operator->() const	{ return m_shader; }
	gl::ShaderObject& operator*() const		{ return *m_shader; }

	/// Destroys old ptr and assigns new one.
	void operator = (gl::ShaderObject* shader);

private:
	gl::ShaderObject* m_shader;
};