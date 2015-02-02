#pragma once

#include <memory>
#include <string>
#include "Time/Stopwatch.h"

class OutputWindow;
class Scene;
class InteractiveCamera;
class Renderer;

class Application
{
public:
	Application(int argc, char** argv);

	~Application();

	void Run();

private:

	void Update(ezTime timeSinceLastUpdate);

	void Draw();

	void Input();

	std::unique_ptr<OutputWindow> m_window;
	std::shared_ptr<Scene> m_scene;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<InteractiveCamera> m_camera;
};
