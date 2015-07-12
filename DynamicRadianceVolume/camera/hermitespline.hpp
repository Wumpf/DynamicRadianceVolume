#pragma once

#include <ei/vector.hpp>
#include <vector>

/// Hermite spline with progress and speed for convenience
class HermiteSpline
{
public:
	HermiteSpline();
	~HermiteSpline() {}

	void SaveToJson(const std::string& filename) const;
	void LoadFromJson(const std::string& filename);

	void Move(float passedTime);
	float GetProgress() const	{ return m_progress; }
	void SetProgress(float f)	{ m_progress = ei::clamp(f, 0.0f, 1.0f); }

	void Evaluate(float t, ei::Vec3& position, ei::Vec3& direction);
	void Evaluate(ei::Vec3& position, ei::Vec3& direction) { Evaluate(m_progress, position, direction); }
	
	void SetLooping(bool loop)		{ m_loop = loop; }
	bool GetLooping() const			{ return m_loop; }

	/// Sets how long it takes to travel the path in seconds from start to end (or start to start for loop=true).
	void SetTravelTime(float travelTime)	{ m_travelTime = travelTime; }
	float GetTravelTime() const				{ return m_travelTime; }


	struct WayPoint
	{
		ei::Vec3 position;
		ei::Vec3 direction;
	};
	const std::vector<WayPoint>& GetWayPoints() const { return m_wayPoints; }
	std::vector<WayPoint>& GetWayPoints() { return m_wayPoints; }

private:
	std::vector<WayPoint> m_wayPoints;

	float m_travelTime; ///< How long it takes to complete the path
	float m_progress;
	bool m_loop;

	static const unsigned int m_pathFileVersion;
};

