#pragma once

#include <memory>
#include <string>
#include "Time/Stopwatch.h"

class OutputWindow;
class Scene;
class InteractiveCamera;
class Renderer;
class AntTweakBarInterface;

class Application
{
public:
	Application(int argc, char** argv);

	~Application();

	void Run();

private:
	void SetupTweakBarBinding();

	void Update();

	void Draw();

	void Input();

	void ChangeLightCount(unsigned int lightCount);

	std::unique_ptr<OutputWindow> m_window;
	std::shared_ptr<Scene> m_scene;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<InteractiveCamera> m_camera;
	std::unique_ptr<AntTweakBarInterface> m_tweakBar;

	ezTime m_timeSinceLastUpdate;
};
