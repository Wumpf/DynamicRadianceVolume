#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <cinttypes>
#include <ei/vector.hpp>
#include <AntTweakBar.h>


struct GLFWwindow;

/// Wrapper for generic AntTweakbar Interface.
///
/// Provides several interop helpers, especially for some type safety and use with std::function callbacks.
/// \attention Do not use AntTweakBar structs as they do not work with the serialization-feature. (also there seem to be bugs with callbacks+twStructs)
class AntTweakBarInterface
{
public:
	/// \attention Redirects glfwwindow callbacks to AntTweakBar
	AntTweakBarInterface(GLFWwindow* glfwWindow);
	~AntTweakBarInterface();

	enum class TypeHint : int
	{
		AUTO,

		/// Ignored for types other than ei::Vec3.
		POSITION, 
		/// Ignored for types other than ei::Vec3.
		HDRCOLOR,
	};

	void Draw();

	void AddButton(const std::string& name, const std::function<void()>& triggerCallback, const std::string& additionalTwDefines = "");
	void AddReadOnly(const std::string& name, const std::function<std::string()>& getValue, const std::string& additionalTwDefines = "");

	/// Read/Write value from get/set callback.
	/// \param typeHint
	///		Use if the AntTweakBar display is ambiguous. For enum types cast the TwType into TypeHint.
	template<typename T>
	void AddReadWrite(const std::string& name, const std::function<T()> &getValue, const std::function<void(const T&)>& setValue, 
						const std::string& additionalTwDefines = "", TypeHint typeHint = TypeHint::AUTO);

	void AddEnumType(const std::string& typeName, const std::vector<TwEnumVal>& values);

	void AddEnum(const std::string& name, const std::string& enumTypeName, const std::function<std::int32_t()> &getValue,
						const std::function<void(std::int32_t)>& setValue, const std::string& additionalTwDefines = "");

	/// Read/Write value from variable.
	/// \param typeHint
	///		Use if the AntTweakBar display is ambiguous. For enum types cast the TwType into TypeHint.
	template<typename T>
	void AddReadWrite(const std::string& name, T& variable, const std::string& twDefines = "", TypeHint typeHint = TypeHint::AUTO);

	void AddSeperator(const std::string& name, const std::string& twDefines = "");

	/// Hides or shows an element.
	void SetVisible(const std::string& name, bool enable);

	/// Helper for defining various group properties.
	void SetGroupProperties(const std::string& groupName, const std::string& parentGroup, const std::string& groupDisplayName, bool opened = false);

	/// Removes an element from AntTweakBar
	void Remove(const std::string& name);


	/// Saves all read/write values to a JSON file.
	/// \attention All custom types will silently be interpreted as int!
	/// This works well for enums, but fails for structs which are not supported at all.
	void SaveReadWriteValuesToJSON(const std::string& jsonFilename);

	void LoadReadWriteValuesToJSON(const std::string& jsonFilename);


	void SetWindowSize(int width, int height);

private:
	void CheckTwError();

	template<typename T>
	ETwType GetTwType(TypeHint hint);

	template<typename T>
	TwGetVarCallback GetGetterFunc();


	struct CTwBar* m_tweakBar;

	/// Base entry type.
	struct EntryBase
	{
		virtual ~EntryBase() {}
		std::string name;
	};
	/// Button entry type.
	struct EntryButton : public EntryBase
	{
		std::function<void()> triggerCallback;
	};
	/// Read only entry.
	struct EntryReadOnly : public EntryBase
	{
		std::function<std::string()> getValue;
	};

	/// General r/w entry.
	struct EntryReadWrite : public EntryBase
	{
		virtual ~EntryReadWrite() {}
		ETwType type;
	};
	/// r/w entry from variable reference
	template<typename T>
	struct EntryReadWriteVar : public EntryReadWrite
	{
		EntryReadWriteVar(T& variable) : variable(variable) {}

		T& variable;
	};
	/// r/w entry from callbacks.
	template<typename T>
	struct EntryReadWriteCB : public EntryReadWrite
	{
		std::function<T()> getValue;
		std::function<void(const T&)> setValue;
	};

	/// Gets value from a r/w entry where underlying type is known but not if EntryReadWriteVar or EntryReadWriteCB.
	template<typename T>
	T GetRWEntryValue(const EntryReadWrite* rwEntry) const;
	/// Sets value from a r/w entry where underlying type is known but not if EntryReadWriteVar or EntryReadWriteCB.
	template<typename InputType>
	void SetRWEntryValue(EntryReadWrite* rwEntry, const InputType& value);
	/// Same as with one template parameter but casts input to CastingAlternative if first call failed.
	template<typename InputType, typename CastingAlternative>
	void SetRWEntryValue(EntryReadWrite* rwEntry, const InputType& value);

	template<typename InputType, typename CastingType>
	bool SetRWEntryValueCast(EntryReadWrite* rwEntry, const InputType& value);

	std::vector<EntryBase*> m_entries;
	TwType m_hdrColorRGBStructType;
	TwType m_positionStructType;

	std::unordered_map<std::string, TwType> m_enumTypes;
};

template<typename T>
inline TwGetVarCallback AntTweakBarInterface::GetGetterFunc()
{
	return [](void *value, void *clientData) {
		*static_cast<T*>(value) = static_cast<EntryReadWriteCB<T>*>(clientData)->getValue();
	};
}
template<>
inline TwGetVarCallback AntTweakBarInterface::GetGetterFunc<std::string>()
{
	return [](void *value, void *clientData) {
		TwCopyStdStringToLibrary(*static_cast<std::string*>(value), static_cast<EntryReadWriteCB<std::string>*>(clientData)->getValue());
	};
}

template<typename T>
inline void AntTweakBarInterface::AddReadWrite(const std::string& name, const std::function<T()>& getValue, const std::function<void(const T&)>& setValue, const std::string& twDefines, AntTweakBarInterface::TypeHint typeHint)
{
	typedef EntryReadWriteCB<T> EntryType;

	EntryType* entry = new EntryType();
	entry->name = name;
	entry->getValue = getValue;
	entry->setValue = setValue;

	TwGetVarCallback getFkt = GetGetterFunc<T>();

	TwSetVarCallback setFkt = [](const void *value, void *clientData) {
		static_cast<EntryType*>(clientData)->setValue(*static_cast<const T*>(value));
	};

	entry->type = AntTweakBarInterface::GetTwType<T>(typeHint);
	m_entries.push_back(entry);

	if (!TwAddVarCB(m_tweakBar, name.c_str(), entry->type, setFkt, getFkt, m_entries.back(), twDefines.c_str()))
		CheckTwError();
}

template<typename T>
inline void AntTweakBarInterface::AddReadWrite(const std::string& name, T& variable, const std::string& twDefines, AntTweakBarInterface::TypeHint typeHint)
{
	TwType type = forceType;
	if (type == TW_TYPE_UNDEF)
		type = AntTweakBarInterface::GetTwType<T>();

	EntryReadWriteVar<T>* entry = new EntryReadWriteVar<T>(variable);
	entry->type = forceType;
	entry->name = name;
	entry->type = AntTweakBarInterface::GetTwType<T>(typeHint);
	m_entries.push_back(entry);

	if (!TwAddVarRW(m_tweakBar, name.c_str(), entry->type, &variable, twDefines.c_str()))
		CheckTwError();
}


template<> inline ETwType AntTweakBarInterface::GetTwType<ei::Vec3>(AntTweakBarInterface::TypeHint hint)
{
	switch (hint)
	{
	case AntTweakBarInterface::TypeHint::POSITION:
		return m_positionStructType;
	case AntTweakBarInterface::TypeHint::HDRCOLOR:
		return m_hdrColorRGBStructType;
	default:
		return TW_TYPE_DIR3F;
	}
}
template<> inline static ETwType AntTweakBarInterface::GetTwType<float>(AntTweakBarInterface::TypeHint) { return TW_TYPE_FLOAT; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<bool>(AntTweakBarInterface::TypeHint) { return TW_TYPE_BOOLCPP; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::string>(AntTweakBarInterface::TypeHint) { return TW_TYPE_STDSTRING; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<ei::Quaternion>(AntTweakBarInterface::TypeHint) { return TW_TYPE_QUAT4F; }
template<> inline static ETwType AntTweakBarInterface::GetTwType<std::int32_t>(AntTweakBarInterface::TypeHint hint)
{
	if (hint == AntTweakBarInterface::TypeHint::AUTO)
		return TW_TYPE_INT32;
	else
		return static_cast<TwType>(hint);
}
template<typename T> inline static ETwType AntTweakBarInterface::GetTwType(AntTweakBarInterface::TypeHint)
{
	LOG_ERROR("Unknown AntTweakBar type!");
	return TW_TYPE_UNDEF;
}
