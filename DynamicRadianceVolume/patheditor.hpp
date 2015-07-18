#pragma once

#include <memory>
#include "Time/Time.h"

class CameraSpline;
class AntTweakBarInterface;
class Application;
struct GLFWwindow;

class PathEditor
{
public:
	PathEditor(Application& application);
	~PathEditor();

	void Draw();
	void Update(ezTime timeSinceLastFrame);

private:

	Application& m_application;

	enum class PathEditTarget
	{
		CAMERA,
		//LIGHT0
	};
	PathEditTarget m_pathEditTarget;
	CameraSpline* m_pathTweakBarEditPath;

	
	std::unique_ptr<AntTweakBarInterface> m_pathTweakBar;

	bool m_recordPath;
	ezTime m_recordInterval;
	ezTime m_timeSinceLastRecord;
};

