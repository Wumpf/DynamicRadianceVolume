#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cinttypes>
#include <ei/vector.hpp>
#include <AntTweakBar.h>


struct GLFWwindow;

/// Wrapper for generic AntTweakbar Interface.
///
/// Provides several interop helpers, especially for some type safety and use with std::function callbacks.
class AntTweakBarInterface
{
public:
	/// \attention Redirects glfwwindow callbacks to AntTweakBar
	AntTweakBarInterface(GLFWwindow* glfwWindow);
	~AntTweakBarInterface();

	void Draw();

	void AddButton(const std::string& name, std::function<void()>& triggerCallback, const std::string& twDefines = "");
	void AddReadOnly(const std::string& name, std::function<std::string()>& getValue, const std::string& twDefines = "");

	/// \forceType
	///		If TW_TYPE_UNDEF, type will be derived using AntTweakBarInterface::GetTwType, otherwise given type will be used.
	template<typename T>
	void AddReadWrite(const std::string& name, std::function<T()> &getValue, std::function<void(const T&)>& setValue, const std::string& twDefines = "", ETwType forceType = TW_TYPE_UNDEF);

	/// \forceType
	///		If TW_TYPE_UNDEF, type will be derived using AntTweakBarInterface::GetTwType, otherwise given type will be use
	template<typename T>
	void AddReadWrite(const std::string& name, T& variable, const std::string& twDefines = "", ETwType forceType = TW_TYPE_UNDEF);

	void AddSeperator(const std::string& name, const std::string& twDefines = "");

	/// Removes an element from AntTweakBar
	void Remove(const std::string& name);

private:
	void CheckTwError();

	template<typename T>
	static ETwType GetTwType();

	struct CTwBar* m_tweakBar;

	struct EntryBase
	{
		std::string name;
	};
	struct EntryButton : public EntryBase
	{
		std::function<void()> triggerCallback;
	};
	struct EntryReadOnly : public EntryBase
	{
		std::function<std::string()> getValue;
	};

	template<typename T>
	struct EntryReadWrite : public EntryBase
	{
		std::function<T()> getValue;
		std::function<void(const T&)> setValue;
	};

	std::vector<EntryBase*> m_entries;
};

template<typename T>
inline void AntTweakBarInterface::AddReadWrite(const std::string& name, std::function<T()> &getValue, std::function<void(const T&)>& setValue, const std::string& twDefines, TwType forceType)
{
	typedef EntryReadWrite<T> EntryType;

	EntryType* entry = new EntryType();
	entry->name = name;
	entry->getValue = getValue;
	entry->setValue = setValue;

	TwGetVarCallback getFkt = [](void *value, void *clientData) {
		*static_cast<T*>(value) = static_cast<EntryType*>(clientData)->getValue();
	};
	TwSetVarCallback setFkt = [](const void *value, void *clientData) {
		static_cast<EntryType*>(clientData)->setValue(*static_cast<const T*>(value));
	};

	m_entries.push_back(entry);

	TwType type = forceType;
	if (type == TW_TYPE_UNDEF)
		type = AntTweakBarInterface::GetTwType<T>();

	if(!TwAddVarCB(m_tweakBar, name.c_str(), type, setFkt, getFkt, m_entries.back(), twDefines.c_str()))
		CheckTwError();
}
template<typename T>
inline void AntTweakBarInterface::AddReadWrite(const std::string& name, T& variable, const std::string& twDefines, ETwType forceType)
{
	TwType type = forceType;
	if (type == TW_TYPE_UNDEF)
		type = AntTweakBarInterface::GetTwType<T>();

	if(!TwAddVarRW(m_tweakBar, name.c_str(), type, &variable, twDefines.c_str()))
		CheckTwError();
}


template<> inline static ETwType AntTweakBarInterface::GetTwType<ei::Vec3>() { return TW_TYPE_DIR3F; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<float>() { return TW_TYPE_FLOAT; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<bool>() { return TW_TYPE_BOOLCPP; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::int32_t>() { return TW_TYPE_INT32; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::uint32_t>() { return TW_TYPE_UINT32; }
template<typename T> inline static ETwType AntTweakBarInterface::GetTwType() { return TW_TYPE_UNDEF; }