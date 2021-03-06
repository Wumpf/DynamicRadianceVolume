#include "anttweakbarinterface.hpp"
#include "utilities/logger.hpp"

#include <glhelper/samplerobject.hpp>

#include <GLFW/glfw3.h>

#include <json/json.h>
#include "utilities/utils.hpp"

#include <algorithm>

void TwEventMouseButtonGLFW3(GLFWwindow* window, int button, int action, int mods){ TwEventMouseButtonGLFW(button, action); }
void TwEventMousePosGLFW3(GLFWwindow* window, double xpos, double ypos){ TwMouseMotion(int(xpos), int(ypos)); }
void TwEventMouseWheelGLFW3(GLFWwindow* window, double xoffset, double yoffset){ TwEventMouseWheelGLFW(static_cast<int>(yoffset)); }
void TwEventKeyGLFW3(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Keymapping changed in GLFW3.
	// Here we handle only a few special cases.
	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_BACKSPACE:
			TwKeyPressed(TW_KEY_BACKSPACE, TW_KMOD_NONE);
			break;
		case GLFW_KEY_LEFT:
			TwKeyPressed(TW_KEY_LEFT, TW_KMOD_NONE);
			break;
		case GLFW_KEY_RIGHT:
			TwKeyPressed(TW_KEY_RIGHT, TW_KMOD_NONE);
			break;
		case GLFW_KEY_UP:
			TwKeyPressed(TW_KEY_UP, TW_KMOD_NONE);
			break;
		case GLFW_KEY_DOWN:
			TwKeyPressed(TW_KEY_DOWN, TW_KMOD_NONE);
			break;
		default:
			TwEventKeyGLFW(key, action);
			break;
		}
	}
}
void TwEventCharGLFW3(GLFWwindow* window, int codepoint){ TwEventCharGLFW(codepoint, GLFW_PRESS); }

bool AntTweakBarInterface::m_twInitialized = false;

AntTweakBarInterface::AntTweakBarInterface(GLFWwindow* glfwWindow, const std::string& barName, bool minimized) :
	m_tweakBar(nullptr),
	m_barName(barName)
{
	if (!m_twInitialized && !TwInit(TW_OPENGL_CORE, NULL))
	{
		LOG_ERROR("AntTweakBar initialization failed: " << TwGetLastError());
	}
	else
	{
		

		// Set GLFW event callbacks
		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height); 
		SetWindowSize(width, height);

		glfwSetMouseButtonCallback(glfwWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW3);
		glfwSetCursorPosCallback(glfwWindow, (GLFWcursorposfun)TwEventMousePosGLFW3);
		glfwSetScrollCallback(glfwWindow, (GLFWscrollfun)TwEventMouseWheelGLFW3);
		glfwSetKeyCallback(glfwWindow, (GLFWkeyfun)TwEventKeyGLFW3);
		glfwSetCharCallback(glfwWindow, (GLFWcharfun)TwEventCharGLFW3);



		// Create a tweak bar
		m_tweakBar = TwNewBar(m_barName.c_str());

		TwDefine((m_barName + " size='350 500' ").c_str());
		TwDefine((m_barName + " valueswidth = 150 ").c_str());
		TwDefine((m_barName + " refresh=0.2 ").c_str());
		TwDefine((m_barName + " contained=true ").c_str()); // TweakBar must be inside the window.
		//TwDefine((m_barName + " alpha=200 ");

		TwDefine((m_barName + " iconified=" + (minimized ? "true" : "false")).c_str());


		if (!m_twInitialized)
		{
			// Type for HDR color
			TwStructMember hdrColorRGBMembers[] =
			{
				{ "R", TW_TYPE_FLOAT, 0, " step=0.1 min=0" },
				{ "G", TW_TYPE_FLOAT, sizeof(float), " step=0.1 min=0" },
				{ "B", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1 min=0" },
			};
			m_hdrColorRGBStructType = TwDefineStruct("HDR Color", hdrColorRGBMembers, ArraySize(hdrColorRGBMembers), sizeof(ei::Vec3), nullptr, nullptr);

			// Type for position
			TwStructMember positionMembers[] =
			{
				{ "X", TW_TYPE_FLOAT, 0, " step=0.1" },
				{ "Y", TW_TYPE_FLOAT, sizeof(float), " step=0.1" },
				{ "Z", TW_TYPE_FLOAT, sizeof(float) * 2, " step=0.1" },
			};
			m_positionStructType = TwDefineStruct("Position", positionMembers, ArraySize(positionMembers), sizeof(ei::Vec3), nullptr, nullptr);
		}

		m_twInitialized = true;
	}
}

AntTweakBarInterface::~AntTweakBarInterface(void)
{
	if (m_tweakBar)
	{
		TwTerminate();
	}

	for (auto entry : m_entries)
		delete entry;
}

void AntTweakBarInterface::SetWindowSize(int width, int height)
{
	TwWindowSize(width, height);
}

void AntTweakBarInterface::CheckTwError()
{
	const char* errorDesc = TwGetLastError();
	if (errorDesc != NULL)
		LOG_ERROR("Tw error: " << errorDesc);
}

void AntTweakBarInterface::AddButton(const std::string& name, const std::function<void()>& triggerCallback, const std::string& twDefines)
{
	EntryButton* entry = new EntryButton();
	entry->name = name;
	entry->triggerCallback = triggerCallback;
	m_entries.push_back(entry);

	TwButtonCallback fkt = [](void* entry) {
		static_cast<EntryButton*>(entry)->triggerCallback();
	};

	if(!TwAddButton(m_tweakBar, name.c_str(), fkt, m_entries.back(), twDefines.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::AddReadOnly(const std::string& name, const std::function<std::string()>& getValue, const std::string& twDefines)
{
	EntryReadOnly* entry = new EntryReadOnly();
	entry->name = name;
	entry->getValue = getValue;
	m_entries.push_back(entry);

	TwGetVarCallback getFkt = [](void *value, void *clientData) {
		std::string* destPtr = static_cast<std::string*>(value);
		TwCopyStdStringToLibrary(*destPtr, static_cast<EntryReadOnly*>(clientData)->getValue());
	};

	TwAddVarCB(m_tweakBar, name.c_str(), TW_TYPE_STDSTRING, NULL, getFkt, m_entries.back(), twDefines.c_str());
	CheckTwError();
}

void AntTweakBarInterface::AddEnumType(const std::string& typeName, const std::vector<TwEnumVal>& values)
{
	if (m_enumTypes.find(typeName) == m_enumTypes.end())
	{
		TwType enumType = TwDefineEnum(typeName.c_str(), values.data(), static_cast<unsigned int>(values.size()));
		m_enumTypes.insert(std::make_pair(typeName, enumType));
	}
	else
	{
		LOG_ERROR("Enum type " << typeName << " already exists!");
	}
}

void AntTweakBarInterface::AddEnum(const std::string& name, const std::string& typeName, const std::function<std::int32_t()> &getValue,
			const std::function<void(std::int32_t)>& setValue, const std::string& additionalTwDefines)
{
	typedef EntryReadWriteCB<std::int32_t> EntryType;

	auto type = m_enumTypes.find(typeName);
	if (type == m_enumTypes.end())
	{
		LOG_ERROR("Enum type " << typeName << " does not exist!");
		return;
	}

	EntryType* entry = new EntryType();
	entry->name = name;
	entry->getValue = getValue;
	entry->setValue = setValue;
	entry->type = type->second;

	TwGetVarCallback getFkt = GetGetterFunc<std::int32_t>();

	TwSetVarCallback setFkt = [](const void *value, void *clientData) {
		static_cast<EntryType*>(clientData)->setValue(*static_cast<const std::int32_t*>(value));
	};

	m_entries.push_back(entry);

	if (!TwAddVarCB(m_tweakBar, name.c_str(), entry->type, setFkt, getFkt, m_entries.back(), additionalTwDefines.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::AddSeperator(const std::string& name, const std::string& twDefines)
{
	if(!TwAddSeparator(m_tweakBar, name.c_str(), twDefines.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::SetVisible(const std::string& name, bool enable)
{
	std::string define = m_barName + "/" + name + " visible=" + (enable ? "true" : "false");
	if (!TwDefine(define.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::SetGroupProperties(const std::string& groupName, const std::string& parentGroup, const std::string& groupDisplayName, bool opened)
{
	std::string define = m_barName + "/" + groupName + " group='" + parentGroup + "' ";
	define += " label= '" + groupDisplayName + "' ";
	define = define + " opened=" + (opened ? "true" : "false");

	if (!TwDefine(define.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::Remove(const std::string& name)
{
	if (!TwRemoveVar(m_tweakBar, name.c_str()))
		CheckTwError();

	auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](EntryBase* entry){ return entry->name == name; });
	if (it != m_entries.end())
		m_entries.erase(it);
}

void AntTweakBarInterface::Draw()
{
	gl::SamplerObject::ResetBinding(0);
	TwDraw();
}

template<typename T>
T AntTweakBarInterface::GetRWEntryValue(const AntTweakBarInterface::EntryReadWrite* rwEntry) const
{
	auto cbEntry = dynamic_cast<const AntTweakBarInterface::EntryReadWriteCB<T>*>(rwEntry);
	if (cbEntry)
		return cbEntry->getValue();
	else
	{
		auto varEntry = dynamic_cast<const AntTweakBarInterface::EntryReadWriteVar<T>*>(rwEntry);
		Assert(varEntry != nullptr, "Invalid type cast to both EntryReadWriteCB and EntryReadWriteVar. Type assumption wrong?");
		return varEntry->variable;
	}
}



template<typename InputType>
void AntTweakBarInterface::SetRWEntryValue(AntTweakBarInterface::EntryReadWrite* rwEntry, const InputType& value)
{
	if(!SetRWEntryValueCast<InputType, InputType>(rwEntry, value))
		Assert(false, "Invalid type cast to both EntryReadWriteCB and EntryReadWriteVar. Type assumption wrong?");
}

template<typename InputType, typename CastingAlternative>
void AntTweakBarInterface::SetRWEntryValue(AntTweakBarInterface::EntryReadWrite* rwEntry, const InputType& value)
{
	if (!SetRWEntryValueCast<InputType, InputType>(rwEntry, value))
	{
		if (!SetRWEntryValueCast<InputType, CastingAlternative>(rwEntry, value))
			Assert(false, "Invalid type cast to both EntryReadWriteCB and EntryReadWriteVar. Type assumption wrong?");
	}
}


template<typename InputType, typename CastingType>
bool AntTweakBarInterface::SetRWEntryValueCast(AntTweakBarInterface::EntryReadWrite* rwEntry, const InputType& value)
{
	auto cbEntry = dynamic_cast<AntTweakBarInterface::EntryReadWriteCB<CastingType>*>(rwEntry);
	if (cbEntry)
		cbEntry->setValue(static_cast<CastingType>(value));
	else
	{
		auto varEntry = dynamic_cast<AntTweakBarInterface::EntryReadWriteVar<CastingType>*>(rwEntry);
		if (!varEntry)
			return false;
		varEntry->variable = static_cast<CastingType>(value);
	}

	return true;
}

void AntTweakBarInterface::SaveReadWriteValuesToJSON(const std::string& jsonFilename)
{
	Json::Value root;

	for (AntTweakBarInterface::EntryBase* entry : m_entries)
	{
		auto rwEntry = dynamic_cast<AntTweakBarInterface::EntryReadWrite*>(entry);
		if (rwEntry)
		{
			char groupname[256] = "";
			TwGetParam(m_tweakBar, rwEntry->name.c_str(), "group", TW_PARAM_CSTRING, 256, groupname);

			Json::Value* value;
			if (strlen(groupname) != 0)
				value = &root[groupname][rwEntry->name];
			else
				value = &root[rwEntry->name];

			TwType type = rwEntry->type;
			// Reinterpret built-in types
			if (type == m_hdrColorRGBStructType || type == m_positionStructType)
				type = TW_TYPE_COLOR3F;

			switch (type)
			{
			case TW_TYPE_INT32:
				*value = GetRWEntryValue<std::int32_t>(rwEntry);
				break;

			case TW_TYPE_BOOLCPP:
				*value = GetRWEntryValue<bool>(rwEntry);
				break;

			case TW_TYPE_COLOR3F:
			case TW_TYPE_DIR3F:
			{	
				ei::Vec3 v = GetRWEntryValue<ei::Vec3>(rwEntry);
				(*value)[0] = v.x;
				(*value)[1] = v.y;
				(*value)[2] = v.z;
				break;
			}

			case TW_TYPE_QUAT4F:
			{
				ei::Quaternion v = GetRWEntryValue<ei::Quaternion>(rwEntry);
				for (unsigned int i = 0; i < 4; ++i)
					(*value)[i] = v.z[i];
				break;
			}

			case TW_TYPE_FLOAT:
				*value = GetRWEntryValue<float>(rwEntry);
				break;

			case TW_TYPE_STDSTRING:
				*value = GetRWEntryValue<std::string>(rwEntry);
				break;

			default:
				// Custom type -> default int
				if (type > TW_TYPE_DIR3D && type < TW_TYPE_CSSTRING(0))
				{
					*value = GetRWEntryValue<int>(rwEntry);
				}
				else
				{
					LOG_WARNING("TweakBar element \"" << rwEntry->name << "/" << groupname << "\" uses an serialization unsupported type!");
				}
			}
		}
	}

	std::ofstream file(jsonFilename.c_str());
	if(file.bad() || !file.is_open())
	{
		LOG_ERROR("Couldn't settings to file " << jsonFilename);
		return;
	}
	file << root;
}

void AntTweakBarInterface::LoadReadWriteValuesToJSON(const std::string& jsonFilename)
{
	std::ifstream file(jsonFilename.c_str());
	if(file.bad() || !file.is_open())
	{
		LOG_ERROR("Couldn't open settings file " << jsonFilename);
		return;
	}
	std::vector<Json::Value> elementQueue;
	elementQueue.emplace_back();
	file >> elementQueue[0];


	std::vector<std::pair<Json::Value, Json::Value>> deferredElements;

	auto setValue = [this](const Json::Value& element, EntryBase* entry)
	{
		if (element.isBool())
			SetRWEntryValue<bool>(static_cast<EntryReadWrite*>(entry), element.asBool());
		else if (element.isInt())
			SetRWEntryValue<std::int32_t, float>(static_cast<EntryReadWrite*>(entry), element.asInt());
		else if (element.isDouble())
			SetRWEntryValue<float, std::int32_t>(static_cast<EntryReadWrite*>(entry), element.asFloat());
		else if (element.isString())
			SetRWEntryValue<std::string>(static_cast<EntryReadWrite*>(entry), element.asString());
		else if (element.isArray() && element.size() == 3 && element[0].isNumeric() && element[1].isNumeric() && element[2].isNumeric())
			SetRWEntryValue<ei::Vec3>(static_cast<EntryReadWrite*>(entry), ei::Vec3(element[0].asFloat(), element[1].asFloat(), element[2].asFloat()));
		else if (element.isArray() && element.size() == 4 && element[0].isNumeric() && element[1].isNumeric() && element[2].isNumeric() && element[3].isNumeric())
			SetRWEntryValue<ei::Quaternion>(static_cast<EntryReadWrite*>(entry), ei::Quaternion(element[0].asFloat(), element[1].asFloat(), element[2].asFloat(), element[3].asFloat()));
		else
			LOG_WARNING("Unkown json attribute type.");
	};

	// Regular list.
	while (!elementQueue.empty())
	{
		Json::Value jsonValue = elementQueue.front();
		elementQueue.erase(elementQueue.begin()); // Principally a bad idea, but we are not performance critical here.

		for (auto childIt = jsonValue.begin(); childIt != jsonValue.end(); ++childIt)
		{
			if(childIt->isObject())
			{
				elementQueue.push_back(*childIt);
			}
			else
			{
				auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&](EntryBase* entry)
					{
						return entry->name == childIt.key().asString() &&
								dynamic_cast<EntryReadWrite*>(entry) != nullptr;
					});
				if (entryIt == m_entries.end())
				{
					deferredElements.push_back(std::make_pair(childIt.key(), *childIt));
				}
				else
					setValue(*childIt, *entryIt);
			}
		}
	}

	// Deferred list.
	for(const auto& it : deferredElements)
	{
		auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&](EntryBase* entry)
		{
			return entry->name == it.first.asString() &&
				dynamic_cast<EntryReadWrite*>(entry) != nullptr;
		});
		if (entryIt != m_entries.end())
			setValue(it.second, *entryIt);
	}
}
