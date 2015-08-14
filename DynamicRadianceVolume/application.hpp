#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Time/Stopwatch.h"

class OutputWindow;
class Scene;
class InteractiveCamera;
class Renderer;
class AntTweakBarInterface;
class FrustumOutlines;
class CameraSpline;
class PathEditor;

class Application
{
public:
	Application(int argc, char** argv);

	~Application();

	void Run();

	OutputWindow& GetWindow() { return *m_window; }
	CameraSpline& GetCameraPath() { return *m_cameraPath; }

	const InteractiveCamera& GetCamera() const { return *m_camera; }

	bool GetCameraFollowPath() const { return m_cameraFollowPath; }
	void SetCameraFollowPath(bool b) { m_cameraFollowPath = b; }

	bool GetUIVisible() const { return m_showTweakBars; }
	void SetUIVisible(bool b) { m_showTweakBars = b; }

	void StartPerfRecording();
	void StopPerfRecording(const std::string& resulftCSVFilename);

	/// If -1, no frametime will be enforced
	/// Otherwise, each frame will be handled as if it took forcedFrametime. This overwrites the frame time measurement.
	void SetOverrideFrametime(float forcedFrametime) { m_overrideFrametime = forcedFrametime; }

	enum class ScreenshotFormat
	{
		PNG,
		JPEG
	};
	void SaveScreenshot(const std::string& filename, ScreenshotFormat format = ScreenshotFormat::PNG);


private:
	void RepopulateTweakBarStatistics();

	void SetupMainTweakBarBinding();

	void Update();

	void Draw();

	void Input();

	// Updates a hard wired test procedure that may be altered on demand
	void UpdateHardwiredMeasureProcedure();


	void ChangeEntityCount(unsigned int entityCount);
	void ChangeLightCount(unsigned int lightCount);

	std::unique_ptr<OutputWindow> m_window;
	std::shared_ptr<Scene> m_scene;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<InteractiveCamera> m_camera;
	std::unique_ptr<AntTweakBarInterface> m_mainTweakBar;

	bool m_cameraFollowPath;
	std::unique_ptr<CameraSpline> m_cameraPath;
	std::unique_ptr<PathEditor> m_pathEditor;

	std::unique_ptr<FrustumOutlines> m_frustumOutlineRenderer;
	bool m_detachViewFromCameraUpdate;

	ezTime m_timeSinceLastUpdate;

	bool m_showTweakBars;

	std::string m_tweakBarStatisticGroupSetting;	
	std::vector<std::string> m_tweakBarStatisticEntries;
	bool m_displayStatAverages;

	float m_overrideFrametime;
};
