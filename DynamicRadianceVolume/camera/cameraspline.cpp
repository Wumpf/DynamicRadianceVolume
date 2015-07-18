#include "cameraspline.hpp"

#include <json/json.h>
#include <fstream>

#include "../utilities/logger.hpp"
#include <ei/elementarytypes.hpp>

const unsigned int CameraSpline::m_pathFileVersion = 0;

CameraSpline::CameraSpline() :
	m_travelTime(10.0f),
	m_progress(0.0f),
	m_loop(false)
{}

void CameraSpline::SaveToJson(const std::string& filename) const
{
	Json::Value jsonRoot;

	jsonRoot["version"] = m_pathFileVersion;
	jsonRoot["traveltime"] = m_travelTime;
	jsonRoot["loop"] = m_loop;

	for (const WayPoint& w : m_wayPoints)
	{
		Json::Value waypoint;
		waypoint["direction"][0] = w.direction.x;
		waypoint["direction"][1] = w.direction.y;
		waypoint["direction"][2] = w.direction.z;
		waypoint["position"][0] = w.position.x;
		waypoint["position"][1] = w.position.y;
		waypoint["position"][2] = w.position.z;
		jsonRoot["waypoints"].append(waypoint);
	}

	std::ofstream file(filename.c_str());
	if (file.bad() || !file.is_open())
	{
		LOG_ERROR("Failed to save camera path to json \"" << filename << "\"");
		return;
	}
	file << jsonRoot;
}

void CameraSpline::LoadFromJson(const std::string& filename)
{
	std::ifstream jsonFile(filename);
	if (jsonFile.bad() || !jsonFile.is_open())
	{
		LOG_ERROR("Failed to load camera path from file \"" << filename << "\"");
		return;
	}
	Json::Value jsonRoot;
	jsonFile >> jsonRoot;
	jsonFile.close();

	int version = jsonRoot.get("version", -1).asInt();
	if (version != m_pathFileVersion)
		LOG_WARNING("Json camera path file version is " << version << " current version is however " << m_pathFileVersion);

	m_travelTime = jsonRoot.get("traveltime", 5.0f).asFloat();
	m_loop = jsonRoot.get("loop", false).asBool();

	m_wayPoints.clear();
	Json::Value waypoints = jsonRoot["waypoints"];
	m_wayPoints.resize(waypoints.size());
	for (Json::Value::ArrayIndex i = 0; i < waypoints.size(); ++i)
	{
		WayPoint& w = m_wayPoints[i];
		w.direction.x = waypoints[i]["direction"][0].asFloat();
		w.direction.y = waypoints[i]["direction"][1].asFloat();
		w.direction.z = waypoints[i]["direction"][2].asFloat();
		w.position.x = waypoints[i]["position"][0].asFloat();
		w.position.y = waypoints[i]["position"][1].asFloat();
		w.position.z = waypoints[i]["position"][2].asFloat();
	}
}

void CameraSpline::Move(float passedTime)
{
	m_progress += passedTime / m_travelTime;
	if (m_loop)
		m_progress -= floorf(m_progress);
	else
		m_progress = ei::clamp(m_progress, 0.0f, 1.0f);
}

void CameraSpline::Evaluate(float t, ei::Vec3& position, ei::Vec3& direction)
{
	if (m_wayPoints.size() == 0)
		return;
	else if (m_wayPoints.size() == 1)
	{
		position = m_wayPoints[0].position;
		direction = m_wayPoints[0].direction;
	}

	int indexn1, index0, indexp1, indexp2;

	t = ei::clamp(t, 0.0f, 1.0f);

	if (m_loop)
	{
		index0 = static_cast<int>(m_wayPoints.size() * m_progress);
		indexp1 = (index0 + 1) % m_wayPoints.size();
		indexp2 = (index0 + 2) % m_wayPoints.size();
		indexn1 = index0 - 1;
		if (indexn1 < 0) indexn1 = static_cast<int>(m_wayPoints.size() - 1);

		t = t * m_wayPoints.size() - index0;
	}
	else
	{
		index0 = static_cast<int>((m_wayPoints.size()-1) * m_progress);
		indexp1 = index0 + 1;
		indexp2 = ei::min(index0 + 2, static_cast<int>(m_wayPoints.size() - 1));
		indexn1 = ei::max(0, index0 - 1);
		t = t * (m_wayPoints.size() - 1) - index0;


		if (indexp1 >= m_wayPoints.size())
			return;
	}

	position =  (2.0f * t * t * t - 3.0f * t * t + 1.0f) * m_wayPoints[index0].position +
				(       t * t * t - 2.0f * t * t + t   ) * (m_wayPoints[indexp1].position - m_wayPoints[indexn1].position) * 0.5f +
			   (-2.0f * t * t * t + 3.0f * t * t       ) * m_wayPoints[indexp1].position +
			    (       t * t * t -        t * t       ) * (m_wayPoints[indexp2].position - m_wayPoints[index0].position) * 0.5f;
	
	// TODO
	/*direction = (6.0f * t * t - 6.0f * t       ) * m_wayPoints[index0].position +
				(3.0f * t * t - 4.0f * t + 1.0f) * m_wayPoints[index0].direction +
			   (-6.0f * t * t + 6.0f * t       ) * m_wayPoints[index1].position +
			    (3.0f * t * t - 2.0f * t       ) * m_wayPoints[index1].direction;*/

//	ei::Vec3 newDirection = ei::slerp(m_wayPoints[index0].direction, m_wayPoints[indexp1].direction, t);

	ei::Vec3 newDirection = (2.0f * t * t * t - 3.0f * t * t + 1.0f) * m_wayPoints[index0].direction +
		(t * t * t - 2.0f * t * t + t) * (m_wayPoints[indexp1].direction - m_wayPoints[indexn1].direction) * 0.5f +
		(-2.0f * t * t * t + 3.0f * t * t) * m_wayPoints[indexp1].direction +
		(t * t * t - t * t) * (m_wayPoints[indexp2].direction - m_wayPoints[index0].direction) * 0.5f;


	float len = ei::len(newDirection);
	if (len > 0.0000001f)
		direction = newDirection / len;
}