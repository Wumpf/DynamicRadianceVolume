#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <glhelper/buffer.hpp>
#include "Time/Stopwatch.h"

/// Profiler module for profiling reoccurring frame events using GPU queries.
///
/// All timing values are given in nanoseconds.
///
/// A note on memory consumption
/// Running an hour day at 100fps: 60*60 * 100 = 8.64e6
/// Sizeof a single record of 20 events: 4 + (4+4) * 20 = 164 bytes
/// -> Takes 56mb
class FrameProfiler
{
private:
	struct OpenQuery
	{
		std::string name;
		unsigned int frame;

		std::uint64_t start;
		std::uint64_t end;

		gl::QueryId startQuery;
		gl::QueryId endQuery;

		bool notEndedYet;
	};

public:
	static FrameProfiler& GetInstance();

	/// Call this function for every frame end.
	///
	/// Will check if queues queries have any results ready, if yes they will be archived and made available through the interface of this class.
	/// If there are any events started but not yet ended, they will be ended and reported as error.
	void OnFrameEnd();

	/// Helper class for scoped queries.
	/// Calls ReportGPUEventStart on construction, ReportGPUEventEnd on destruction
	class ScopedQuery
	{
	public:
		ScopedQuery(std::string eventIdentifier) : eventIdentifier(eventIdentifier) { FrameProfiler::GetInstance().ReportGPUEventStart(eventIdentifier); }
		~ScopedQuery() { FrameProfiler::GetInstance().ReportGPUEventEnd(eventIdentifier); }
	private:
		std::string eventIdentifier;
	};

	/// Reports the start of a given GPU event.
	///
	/// You are expected to reuse the same type for every frame. (e.g. "GBufferFill")
	/// Triggers glBeginQuery.
	/// Multiple events with the same type may be reported during a frame. However, it is not possible to nest events with the same name!
	/// Events with the same type within a frame will be listed in order of completion.
	void ReportGPUEventStart(const std::string& eventType);
	/// Reports the end of a given event.
	///
	/// Triggers glEndQuery. Note that a is at the earliest available on the next OnFrameStart. However, it may take longer.
	void ReportGPUEventEnd(const std::string& eventType);

	/// Closes all open queries (if any) and waits for results.
	void WaitForQueryResults();

	struct Event
	{
		Event(const OpenQuery& e) : frame(e.frame), duration(static_cast<std::uint32_t>(e.end - e.start)) {}

		/// Frame in which this event occurred.
		unsigned int frame;

		/// Duration of the event in nanoseconds.
		std::uint32_t duration;
	};

	const std::vector<std::uint32_t>& GetFrameDurations() const										{ return m_recordedFrameDurations; }

	/// Retrieves event list for all event types.
	///
	/// Guarantees that in every list the Event::frame is ascending.
	const std::vector<std::pair<std::string, std::vector<Event>>> &GetAllRecordedEvents() const		{ return m_eventLists; }

	/// Saves all statistics to a CSV file.
	///
	/// Calls WaitForQueries internally.
	void SaveToCSV(const std::string& filename);

private:
	FrameProfiler();
	~FrameProfiler();

	void RetrieveQueryResults();

	gl::QueryId GetQueryFromPool();
	void ReturnQueryToPool(gl::QueryId unusedQuery);

	ezStopwatch m_frameStopwatch;

	unsigned int m_lastCompletedFrame;
	std::vector<std::uint32_t> m_recordedFrameDurations;
	std::vector<std::pair<std::string, std::vector<Event>>> m_eventLists;


	/// Currently unused timer queries.
	std::vector<GLuint> m_timerQueryPool;


	/// Queries for which either ReportGPUEventEnd was not yet called, or results are not yet available.
	std::vector<OpenQuery> m_openQueries;

	/// How many new queries are allocated when the pool is empty.
	static const unsigned int s_queryPoolAllocationSize = 64;
};


// Macros for queries for easy removal.
#define GPU_PROFILING

#ifdef GPU_PROFILING
	#define PROFILE_GPU_EVENT_SCOPED(name) FrameProfiler::ScopedQuery query_##name(#name);
	#define PROFILE_GPU_EVENT_START(name) FrameProfiler::GetInstance().ReportGPUEventStart(#name);
	#define PROFILE_GPU_EVENT_END(name) FrameProfiler::GetInstance().ReportGPUEventEnd(#name);
#else
	#define PROFILE_GPU_EVENT_SCOPED(name)
	#define PROFILE_GPU_EVENT_START(name)
	#define PROFILE_GPU_EVENT_END(name)
#endif
