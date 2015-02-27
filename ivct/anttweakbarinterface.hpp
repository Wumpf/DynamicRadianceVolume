#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cinttypes>
#include <ei/vector.hpp>
#include <AntTweakBar.h>


struct GLFWwindow;

class AntTweakBarInterface
{
public:
	/// \attention Redirects glfwwindow callbacks to AntTweakBar
	AntTweakBarInterface(GLFWwindow* glfwWindow);
	~AntTweakBarInterface();

	void Draw();

	void AddButton(const std::string& name, std::function<void()>& triggerCallback, const std::string& twDefines = "");
	void AddReadOnly(const std::string& name, std::function<std::string()>& getValue, const std::string& twDefines = "");

	template<typename T>
	void AddReadWrite(const std::string& name, std::function<T()> &getValue, std::function<void(const T&)>& setValue, const std::string& twDefines = "");

	void AddSeperator(const std::string& name, const std::string& twDefines = "");

private:
	void CheckTwError();

	template<typename T>
	static ETwType GetTwType();

	struct CTwBar* m_tweakBar;

	struct EntryBase
	{
		std::string name;
		std::string category;
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
inline void AntTweakBarInterface::AddReadWrite(const std::string& name, std::function<T()> &getValue, std::function<void(const T&)>& setValue, const std::string& twDefines)
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

	TwAddVarCB(m_tweakBar, name.c_str(), AntTweakBarInterface::GetTwType<T>(), setFkt, getFkt, m_entries.back(), twDefines.c_str());
	CheckTwError();
}


template<> inline static ETwType AntTweakBarInterface::GetTwType<ei::Vec3>() { return TW_TYPE_DIR3F; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<float>() { return TW_TYPE_FLOAT; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<bool>() { return TW_TYPE_BOOLCPP; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::int32_t>() { return TW_TYPE_INT32; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::uint32_t>() { return TW_TYPE_UINT32; }