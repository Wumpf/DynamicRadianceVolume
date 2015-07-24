#include "frameprofiler.hpp"
#include <memory>
#include <chrono>
#include <thread>

FrameProfiler& FrameProfiler::GetInstance()
{
	static FrameProfiler profiler;
	return profiler;
}

FrameProfiler::FrameProfiler() :
	m_gpuProfilingActive(true)
{
}

FrameProfiler::~FrameProfiler()
{
}

void FrameProfiler::OnFrameEnd()
{
	m_recordedFrameDurations.push_back(static_cast<std::uint32_t>(m_frameStopwatch.Checkpoint().GetNanoseconds()));
	RetrieveQueryResults();
}

void FrameProfiler::RetrieveQueryResults()
{
	Event evt;

	// Traverse from old to new.
	for (auto it = m_openQueries.begin(); it != m_openQueries.end(); ++it)
	{
		evt.frame = it->frame;
		std::uint32_t durationNS = std::numeric_limits<std::uint32_t>::max();
		GL_CALL(glGetQueryObjectuiv, it->query, GL_QUERY_RESULT_NO_WAIT, &durationNS);

		if (durationNS != std::numeric_limits<std::uint32_t>::max())
		{
			evt.value = static_cast<float>(static_cast<double>(durationNS) / 1000.0 / 1000.0);

			ReturnQueryToPool(it->query);

			// Archive.
			auto archiveIt = m_eventLists.begin();
			for (; archiveIt != m_eventLists.end(); ++archiveIt)
			{
				if (archiveIt->first == it->name) break;
			}
			Assert(archiveIt != m_eventLists.end(), "Eventtype was not yet created! Should have happened in ReportGPUEventStart.");

			// It may happen that the query of a newer event was retrieved earlier!
			auto insertPosition = archiveIt->second.crbegin();
			while (insertPosition != archiveIt->second.crend() && insertPosition->frame > it->frame)
				++insertPosition;

			archiveIt->second.insert(insertPosition.base(), evt);
			Assert(archiveIt->second.size() == 1 || archiveIt->second[archiveIt->second.size() - 2].frame <= archiveIt->second[archiveIt->second.size() - 1].frame, "Events are not ordered by frame id!"); // Just to be sure the above is right.

			// Remove from open list.
			it = m_openQueries.erase(it);
			if (it == m_openQueries.end()) break;
			continue;
		}
	}
}

void FrameProfiler::StartQuery(const std::string& eventType)
{
	if (!m_gpuProfilingActive) return;

	if (m_awaitingQueryEndCall)
	{
		LOG_ERROR("It is not possible to nest GPU time elapsed queries! Event type: \"" << eventType << "\"");
		return;
	}

	// Event type known?
	auto archiveIt = m_eventLists.begin();
	for (; archiveIt != m_eventLists.end(); ++archiveIt)
	{
		if (archiveIt->first == eventType) break;
	}
	if (archiveIt == m_eventLists.end())
		m_eventLists.emplace_back(eventType, std::vector<Event>());

	// Add
	m_awaitingQueryEndCall = true;
	m_activeQuery.name = eventType;
	m_activeQuery.frame = static_cast<unsigned int>(m_recordedFrameDurations.size());
	m_activeQuery.query = GetQueryFromPool();
	GL_CALL(glBeginQuery, GL_TIME_ELAPSED, m_activeQuery.query);
}

void FrameProfiler::EndQuery()
{
	if (!m_gpuProfilingActive) return;

	if (!m_awaitingQueryEndCall)
	{
		LOG_ERROR("No query started yet!");
		return;
	}

	GL_CALL(glEndQuery, GL_TIME_ELAPSED);
	m_awaitingQueryEndCall = false;
	m_openQueries.push_back(m_activeQuery);
}

void FrameProfiler::ReportValue(const std::string& eventType, float value)
{
	auto archiveIt = m_eventLists.begin();
	for (; archiveIt != m_eventLists.end(); ++archiveIt)
	{
		if (archiveIt->first == eventType)
			break;
	}

	if (archiveIt == m_eventLists.end())
	{
		m_eventLists.emplace_back(eventType, std::vector<Event>());
		archiveIt = m_eventLists.end() - 1;
	}

	Event evt;
	evt.frame = static_cast<unsigned int>(m_recordedFrameDurations.size());
	evt.value = value;
	archiveIt->second.push_back(evt);
}

void FrameProfiler::WaitForQueryResults()
{
	while (m_openQueries.size())
	{
		RetrieveQueryResults();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

gl::QueryId FrameProfiler::GetQueryFromPool()
{
	if (m_timerQueryPool.empty())
	{
		m_timerQueryPool.resize(s_queryPoolAllocationSize);
		GL_CALL(glCreateQueries, GL_TIME_ELAPSED, s_queryPoolAllocationSize, m_timerQueryPool.data());
	}

	gl::QueryId queryId = m_timerQueryPool.back();
	m_timerQueryPool.pop_back();
	return queryId;
}

void FrameProfiler::ReturnQueryToPool(gl::QueryId unusedQuery)
{
	m_timerQueryPool.push_back(unusedQuery);
}

void FrameProfiler::ComputeAverages()
{
	m_averages.resize(m_eventLists.size());

	for (size_t i = 0; i < m_eventLists.size(); ++i)
	{
		double avg = 0.0;
		for (const Event& evt : m_eventLists[i].second)
			avg += evt.value;

		m_averages[i].first = m_eventLists[i].first;
		m_averages[i].second  = static_cast<float>(avg / m_eventLists[i].second.size());
	}
}

void FrameProfiler::SaveToCSV(const std::string& filename)
{
	WaitForQueryResults();

	std::ofstream csvFile(filename);
	if (csvFile.bad() || !csvFile.is_open())
	{
		LOG_ERROR("Could not open \"" << filename << "\" for writing profile data");
		return;
	}

	// Column names
	std::unique_ptr<size_t[]> nextEvtIndex(new size_t[m_eventLists.size()]);
	csvFile << "frame,";
	for (size_t i = 0; i < m_eventLists.size(); ++i)
	{
		nextEvtIndex[i] = 0;
		csvFile << "\"" << m_eventLists[i].first << "\"";
		if (i != m_eventLists.size() - 1)
			csvFile << ",";
	}
	csvFile << "\n";

	// Data
	for (size_t frame = 0; frame < m_recordedFrameDurations.size(); ++frame)
	{
		csvFile << static_cast<double>(m_recordedFrameDurations[frame]) / 1000.0 / 1000.0 << ",";

		for (size_t evtTypeIdx = 0; evtTypeIdx < m_eventLists.size(); ++evtTypeIdx)
		{
			const auto& events = m_eventLists[evtTypeIdx].second;

			if (nextEvtIndex[evtTypeIdx] < events.size() && events[nextEvtIndex[evtTypeIdx]].frame == frame)
			{
				csvFile << events[nextEvtIndex[evtTypeIdx]].value;
				++nextEvtIndex[evtTypeIdx];
			}
			else
				csvFile << "0";

			if (evtTypeIdx != m_eventLists.size() - 1)
				csvFile << ",";
		}

		csvFile << "\n";
	}

	LOG_INFO("Wrote frameprofiler statistics as CSV to " << filename);
}