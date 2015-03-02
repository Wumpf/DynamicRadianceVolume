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

AntTweakBarInterface::AntTweakBarInterface(GLFWwindow* glfwWindow) :
	m_tweakBar(nullptr)
{
	if (!TwInit(TW_OPENGL_CORE, NULL))
	{
		LOG_ERROR("AntTweakBar initialization failed: " << TwGetLastError());
	}
	else
	{
		// Set GLFW event callbacks
		int width, height;
		glfwGetWindowSize(glfwWindow, &width, &height); 
		TwWindowSize(width, height);

		glfwSetMouseButtonCallback(glfwWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW3);
		glfwSetCursorPosCallback(glfwWindow, (GLFWcursorposfun)TwEventMousePosGLFW3);
		glfwSetScrollCallback(glfwWindow, (GLFWscrollfun)TwEventMouseWheelGLFW3);
		glfwSetKeyCallback(glfwWindow, (GLFWkeyfun)TwEventKeyGLFW3);
		glfwSetCharCallback(glfwWindow, (GLFWcharfun)TwEventCharGLFW3);



		// Create a tweak bar
		m_tweakBar = TwNewBar("TweakBar");

		TwDefine(" TweakBar size='300 300' ");
		TwDefine(" TweakBar valueswidth=150 ");
		TwDefine(" TweakBar refresh=0.2 ");
		TwDefine(" TweakBar contained=true "); // TweakBar must be inside the window.
		//TwDefine(" TweakBar alpha=200 ");


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


void AntTweakBarInterface::AddSeperator(const std::string& name, const std::string& twDefines)
{
	if(!TwAddSeparator(m_tweakBar, name.c_str(), twDefines.c_str()))
		CheckTwError();
}

void AntTweakBarInterface::SetGroupProperties(const std::string& subgroupName, const std::string& parentGroup, const std::string& subgroupDisplayName, bool opened)
{
	std::string define = " TweakBar/" + subgroupName + " group='" + parentGroup + "' ";
	define += " label= '" + subgroupDisplayName + "' ";
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

template<typename T>
void AntTweakBarInterface::SetRWEntryValue(AntTweakBarInterface::EntryReadWrite* rwEntry, const T& value)
{
	auto cbEntry = dynamic_cast<AntTweakBarInterface::EntryReadWriteCB<T>*>(rwEntry);
	if (cbEntry)
		return cbEntry->setValue(value);
	else
	{
		auto varEntry = dynamic_cast<AntTweakBarInterface::EntryReadWriteVar<T>*>(rwEntry);
		Assert(varEntry != nullptr, "Invalid type cast to both EntryReadWriteCB and EntryReadWriteVar. Type assumption wrong?");
		varEntry->variable = value;
	}
}

void AntTweakBarInterface::SaveReadWriteValuesToJSON(const std::string& jsonFilename)
{
	Json::Value root;

	for (AntTweakBarInterface::EntryBase* entry : m_entries)
	{
		auto rwEntry = dynamic_cast<AntTweakBarInterface::EntryReadWrite*>(entry);
		if (rwEntry)
		{
			char groupname[256];
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
			case TW_TYPE_UINT32:
				*value = GetRWEntryValue<std::uint32_t>(rwEntry);
				break;

			case TW_TYPE_BOOL32:
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

			case TW_TYPE_FLOAT:
				*value = GetRWEntryValue<float>(rwEntry);
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
	file << root;
}

#include <iostream>
void AntTweakBarInterface::LoadReadWriteValuesToJSON(const std::string& jsonFilename)
{
	std::ifstream file(jsonFilename.c_str());
	std::vector<Json::Value> elementStack;
	elementStack.emplace_back();
	file >> elementStack[0];


	while (!elementStack.empty())
	{
		Json::Value jsonValue = elementStack.back();
		elementStack.pop_back();

		for (auto childIt = jsonValue.begin(); childIt != jsonValue.end(); ++childIt)
		{
			if(childIt->isObject())
			{
				elementStack.push_back(*childIt);
			}
			else
			{
				auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&](EntryBase* entry)
					{
						return entry->name == childIt.key().asString() &&
								dynamic_cast<EntryReadWrite*>(entry) != nullptr;
					});
				if (entryIt == m_entries.end())
					continue;
				
				if (childIt->isBool())
					SetRWEntryValue<bool>(static_cast<EntryReadWrite*>(*entryIt), childIt->asBool());
				else if (childIt->isInt())
					SetRWEntryValue<int>(static_cast<EntryReadWrite*>(*entryIt), childIt->asInt());
				else if (childIt->isUInt())
					SetRWEntryValue<unsigned int>(static_cast<EntryReadWrite*>(*entryIt), childIt->asUInt());
				else if (childIt->isDouble())
					SetRWEntryValue<float>(static_cast<EntryReadWrite*>(*entryIt), childIt->asFloat());
				else if (childIt->isArray() && childIt->size() == 3 && (*childIt)[0].isDouble() && (*childIt)[1].isDouble() && (*childIt)[2].isDouble())
					SetRWEntryValue<ei::Vec3>(static_cast<EntryReadWrite*>(*entryIt), ei::Vec3((*childIt)[0].asFloat(), (*childIt)[1].asFloat(), (*childIt)[2].asFloat()));
				else
					LOG_WARNING("Unkown json attribute type at \"" << childIt.key().asCString() << "\"");
			}
		}
	}
}
