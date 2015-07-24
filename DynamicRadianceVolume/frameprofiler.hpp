#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <glhelper/buffer.hpp>
#include "Time/Stopwatch.h"

/// Profiler module for profiling the GPU time of given queries.
///
/// All timing values are given in nanoseconds.
/// 
/// A note on memory consumption:
/// Running an hour day at 100fps: 60*60 * 100 = 8.64e6
/// Sizeof a single record of 20 events: 4 + (4+4) * 20 = 164 bytes
/// -> Takes 56mb
class FrameProfiler
{
private:
	struct OpenGPUQuery
	{
		std::string name;
		unsigned int frame;
		gl::QueryId query;
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
		ScopedQuery(std::string eventIdentifier) : eventIdentifier(eventIdentifier) { FrameProfiler::GetInstance().StartQuery(eventIdentifier); }
		~ScopedQuery() { FrameProfiler::GetInstance().EndQuery(); }
	private:
		std::string eventIdentifier;
	};


	/// On/off switch for GPU queries.
	///
	/// Useful for estimating performance impact of queries. For a exact check you should remove the profiling calls entirely!
	void SetGPUProfilingActive(bool active) { m_gpuProfilingActive = active; }
	bool GetGPUProfilingActive() const		{ return m_gpuProfilingActive; }

	/// Reports the start of a given GPU event.
	///
	/// You are expected to reuse the same type for every frame (e.g. "GBufferFill").
	/// It is NOT possible to nest queries!
	/// Triggers glBeginQuery. "The timer starts when the first command starts executing, and the timer ends when the final command is completed." (https://www.opengl.org/wiki/Query_Object#Timer_queries)
	/// Multiple events with the same type may be reported during a frame.
	/// Events with the same type within a frame will be listed in order of completion.
	void StartQuery(const std::string& eventType);
	/// Reports the end of last event.
	///
	/// Triggers glEndQuery. Note that a is at the earliest available on the next OnFrameStart. However, it may take longer.
	void EndQuery();

	/// Reports a timing/statistics/... value directly without any GPU queries etc.
	/// Can be used for various measurements results etc.
	void ReportValue(const std::string& eventType, float value);

	/// Clears all statistics.
	void Clear() { m_recordedFrameDurations.clear(); m_eventLists.clear(); }

	/// Closes all open queries (if any) and waits for results.
	void WaitForQueryResults();

	struct Event
	{
		/// Frame in which this event occurred.
		unsigned int frame;

		/// Event value. Usually the duration of the event in nanoseconds.
		float value;
	};
	typedef std::pair<std::string, std::vector<FrameProfiler::Event>> EventList;

	const std::vector<std::uint32_t>& GetFrameDurations() const		{ return m_recordedFrameDurations; }

	/// Retrieves event list for all event types.
	///
	/// Guarantees that in every list the Event::frame is ascending.
	const std::vector<EventList>& GetAllRecordedEvents() const		{ return m_eventLists; }


	/// Computes averages for all events over all recorded frames.
	void ComputeAverages();
	const std::vector<std::pair<std::string, float>>& GetAverages() const { return m_averages; }

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

	std::vector<std::uint32_t> m_recordedFrameDurations;
	std::vector<EventList> m_eventLists;

	std::vector<std::pair<std::string, float>> m_averages;

	bool m_gpuProfilingActive;

	/// Currently unused timer queries.
	std::vector<GLuint> m_timerQueryPool;

	/// True if m_activeQuery contains a started but not ended query.
	bool m_awaitingQueryEndCall;
	OpenGPUQuery m_activeQuery;

	/// Queries for which results are awaited.
	std::vector<OpenGPUQuery> m_openQueries;

	/// How many new queries are allocated when the pool is empty.
	static const unsigned int s_queryPoolAllocationSize = 64;
};


// Macros for queries for preprocessor switchable profiling.
#define GPU_PROFILING

#ifdef GPU_PROFILING
	#define PROFILE_GPU_SCOPED(name) FrameProfiler::ScopedQuery query_##name(#name);
	#define PROFILE_GPU_START(name) FrameProfiler::GetInstance().StartQuery(#name);
	#define PROFILE_GPU_END() FrameProfiler::GetInstance().EndQuery();
#else
	#define PROFILE_GPU_SCOPED(name)
	#define PROFILE_GPU_START(name)
	#define PROFILE_GPU_END()
#endif
