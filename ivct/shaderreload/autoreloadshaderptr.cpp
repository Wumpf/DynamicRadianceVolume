#pragma once

#include "shaderfilewatcher.hpp"
#include "autoreloadshaderptr.hpp"
#include <glhelper/shaderobject.hpp>

AutoReloadShaderPtr::AutoReloadShaderPtr(gl::ShaderObject* shader) : m_shader(shader)
{
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shader);
}

AutoReloadShaderPtr::~AutoReloadShaderPtr()
{
	if (m_shader)
	{
		ShaderFileWatcher::Instance().UnregisterShaderForReloadOnChange(m_shader);
		delete m_shader;
	}
}

void AutoReloadShaderPtr::operator = (gl::ShaderObject* shader)
{
	this->~AutoReloadShaderPtr();

	m_shader = shader;
	ShaderFileWatcher::Instance().RegisterShaderForReloadOnChange(m_shader);
}