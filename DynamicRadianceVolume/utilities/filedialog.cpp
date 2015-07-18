#include "filedialog.hpp"

#undef APIENTRY
#include <windows.h>
#include <shlwapi.h>


std::string OpenFileDialog()
{
	// Openfiledialog changes relative paths. Save current directory to restore it later.
	char oldCurrentPath[MAX_PATH] = "";
	GetCurrentDirectoryA(MAX_PATH, oldCurrentPath);

	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	//ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (GetOpenFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		char relativePath[MAX_PATH];
		if (PathRelativePathToA(relativePath, oldCurrentPath, FILE_ATTRIBUTE_DIRECTORY, filename, FILE_ATTRIBUTE_NORMAL))
			return relativePath;
		else
			return filename;
	}
	else
	{
		return "";
	}
}

std::string SaveFileDialog(const std::string& defaultName, const std::string& fileEnding)
{
	// Openfiledialog changes relative paths. Save current directory to restore it later.
	char oldCurrentPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, oldCurrentPath);

	char filename[MAX_PATH];
	strcpy(filename, defaultName.c_str());

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	//ofn.hwndOwner = m_window->GetWindowHandle();
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0Json Files (*.json)\0*.json\0Obj Model (*.obj)\0*.obj\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = fileEnding.c_str();

	if (GetSaveFileNameA(&ofn))
	{
		// Restore current directory
		SetCurrentDirectoryA(oldCurrentPath);

		char relativePath[MAX_PATH];
		if (PathRelativePathToA(relativePath, oldCurrentPath, FILE_ATTRIBUTE_DIRECTORY, filename, FILE_ATTRIBUTE_NORMAL))
			return relativePath;
		else
			return filename;
	}
	else
	{
		return "";
	}
}