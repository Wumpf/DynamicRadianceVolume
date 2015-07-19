#include "patheditor.hpp"
#include "application.hpp"
#include "anttweakbarinterface.hpp"
#include "outputwindow.hpp"

#include "camera/interactivecamera.hpp"
#include "camera/cameraspline.hpp"
#include "utilities/filedialog.hpp"


PathEditor::PathEditor(Application& application) :
	m_application(application),

	m_recordPath(false),
	m_recordInterval(ezTime::Seconds(0.5f)),
	m_timeSinceLastRecord(),

	m_recordingPerf(false)
{
	m_pathTweakBar = std::make_unique<AntTweakBarInterface>(application.GetWindow().GetGLFWWindow(), "Path", true);
	application.GetWindow().AddResizeHandler([&](int width, int height){
		m_pathTweakBar->SetWindowSize(width, height);
	});


	/*std::vector<TwEnumVal> targetEnum = { { static_cast<int>(PathEditTarget::CAMERA), "Camera" } }; // , { static_cast<int>(PathEditTarget::LIGHT0), "Light0" }
	m_pathTweakBar->AddEnumType("PathTarget", targetEnum);
	m_pathTweakBar->AddEnum("Path Target", "PathTarget", [&](){ return static_cast<int>(m_pathEditTarget); }, [&](int i){
		m_pathEditTarget = static_cast<PathEditTarget>(i);
		if (m_pathEditTarget == PathEditTarget::CAMERA)
			m_pathTweakBarEditPath = &m_application.GetCameraPath();
	//	else if (m_pathEditTarget == PathEditTarget::LIGHT0 && m_scene->GetLights().size() > 0)
	//		m_pathTweakBarEditPath = &m_scene->GetLights()[0].path;
	}); */
	m_pathTweakBarEditPath = &m_application.GetCameraPath();

	m_pathTweakBar->AddButton("Save Path", [&]() { m_pathTweakBarEditPath->SaveToJson(SaveFileDialog("path.json", ".json")); });
	m_pathTweakBar->AddButton("Load Path", [&]() { m_pathTweakBarEditPath->LoadFromJson(OpenFileDialog()); });
	m_pathTweakBar->AddSeperator("main");

	m_pathTweakBar->AddReadWrite<bool>("Follow Path", [&]() {
		if (m_pathEditTarget == PathEditTarget::CAMERA)
			return m_application.GetCameraFollowPath();
		/*else if (m_pathEditTarget == PathEditTarget::LIGHT0 && m_scene->GetLights().size() > 0)
			return m_scene->GetLights()[0].followPath;*/
		else
			return false;
	}, [&](bool b) {
		if (m_pathEditTarget == PathEditTarget::CAMERA)
			m_application.SetCameraFollowPath(b);
		/*else if (m_pathEditTarget == PathEditTarget::LIGHT0 && m_scene->GetLights().size() > 0)
			m_scene->GetLights()[0].followPath = b;*/
	});

	m_pathTweakBar->AddReadWrite<bool>("Loop", [&]() { return m_pathTweakBarEditPath->GetLooping(); }, [&](bool b) { m_pathTweakBarEditPath->SetLooping(b); });
	m_pathTweakBar->AddReadWrite<float>("TravelTime", [&]{ return m_pathTweakBarEditPath->GetTravelTime(); }, [&](float f){ m_pathTweakBarEditPath->SetTravelTime(f); }, "min=0 max=1000 step=0.1");
	m_pathTweakBar->AddReadWrite<float>("Progress", [&]{ return m_pathTweakBarEditPath->GetProgress(); }, [&](float f){ m_pathTweakBarEditPath->SetProgress(f); }, "min=0 max=1 step=0.001");


	m_pathTweakBar->AddSeperator("control");

	m_pathTweakBar->AddReadOnly("# way-points", [&]{ return std::to_string(m_pathTweakBarEditPath->GetWayPoints().size()); });
	m_pathTweakBar->AddButton("Add way-point at View", [&]{ m_pathTweakBarEditPath->GetWayPoints().push_back(CameraSpline::WayPoint{ m_application.GetCamera().GetPosition(), m_application.GetCamera().GetDirection() }); });
	
	m_pathTweakBar->AddReadWrite<bool>("Record", [&]() { return m_recordPath; }, [&](bool b) { m_recordPath = b; m_timeSinceLastRecord = m_recordInterval; });
	m_pathTweakBar->AddReadWrite<float>("Record Interval", [&]() { return static_cast<float>(m_recordInterval.GetSeconds()); }, [&](float f) { m_recordInterval = ezTime::Seconds(f); }, "min=0.05 max=4 step=0.05");
	
	m_pathTweakBar->AddButton("Remove last way-point", [&]{ if (!m_pathTweakBarEditPath->GetWayPoints().empty()) m_pathTweakBarEditPath->GetWayPoints().pop_back(); });
	m_pathTweakBar->AddButton("Clear way-points", [&]{ m_pathTweakBarEditPath->GetWayPoints().clear(); });

	m_pathTweakBar->AddSeperator("path");

	m_pathTweakBar->AddButton("Record Performance on Path", [&]{
		if (!m_recordingPerf)
		{
			m_pathTweakBarEditPath->SetProgress(0.0f);
			m_pathTweakBarEditPath->SetLooping(false);
			if (m_pathEditTarget == PathEditTarget::CAMERA)
				m_application.SetCameraFollowPath(true);
			m_application.SetUIVisible(false);
			m_application.StartPerfRecording();
			m_recordingPerf = true;
		}
	});

}

PathEditor::~PathEditor() {}


void PathEditor::Update(ezTime timeSinceLastFrame)
{
	if (m_recordPath)
	{
		m_timeSinceLastRecord += timeSinceLastFrame;
		if (m_timeSinceLastRecord >= m_recordInterval)
		{
			m_timeSinceLastRecord -= m_recordInterval;
			m_pathTweakBarEditPath->GetWayPoints().push_back(CameraSpline::WayPoint{ m_application.GetCamera().GetPosition(), m_application.GetCamera().GetDirection() });
		}
	}

	if (m_recordingPerf && m_pathTweakBarEditPath->GetProgress() >= 1.0f)
	{
		m_recordingPerf = false;
		m_application.SetUIVisible(true);
		m_application.StopPerfRecording("path_perf.csv");
	}
}