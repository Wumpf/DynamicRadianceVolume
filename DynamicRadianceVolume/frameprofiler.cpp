#include "frameprofiler.hpp"
#include <memory>
#include <chrono>
#include <thread>

FrameProfiler& FrameProfiler::GetInstance()
{
	static FrameProfiler profiler;
	return profiler;
}

FrameProfiler::FrameProfiler()
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
	// Traverse from old to new.
	for (auto it = m_openQueries.begin(); it != m_openQueries.end(); ++it)
	{
		if (it->notEndedYet)
		{
			LOG_ERROR("Event from previous frame \"" << it->name << "\" was not yet ended. You should call ReportGPUEventEnd for all event before starting a new frame.");
			ReportGPUEventEnd(it->name);
			continue; // Do not process this now, since there might be multiple queries with this name - the function could have closed the wrong one!
		}

		if (it->start == 0)
		{
			GL_CALL(glGetQueryObjectui64v, it->startQuery, GL_QUERY_RESULT_NO_WAIT, &it->start);
			if (it->start != 0)
			{
				ReturnQueryToPool(it->startQuery);
				it->startQuery = 0;
			}
		}
		if (it->end == 0)
		{
			GL_CALL(glGetQueryObjectui64v, it->endQuery, GL_QUERY_RESULT_NO_WAIT, &it->end);
			if (it->end != 0)
			{
				ReturnQueryToPool(it->endQuery);
				it->endQuery = 0;
			}
		}

		// Both start end end have valid values now?
		if (it->start != 0 && it->end != 0)
		{
			// Archive.
			auto archiveIt = m_eventLists.begin();
			for (; archiveIt != m_eventLists.end(); ++archiveIt)
			{
				if (archiveIt->first == it->name) break;
			}
			if (archiveIt == m_eventLists.end())
				m_eventLists.emplace_back(it->name, std::vector<Event>({ Event(*it) }));
			else
			{
				// It may happen that the query of a newer event was retrieved earlier!
				auto insertPosition = archiveIt->second.crbegin();
				while(insertPosition != archiveIt->second.crend() && insertPosition->frame > it->frame)
					++insertPosition;

				archiveIt->second.insert(insertPosition.base(), Event(*it));
				Assert(archiveIt->second[archiveIt->second.size() - 2].frame <= archiveIt->second[archiveIt->second.size() - 1].frame, "Events are not ordered by frame id!"); // Just to be sure the above is right.
			}

			// Remove from open list.
			it = m_openQueries.erase(it);
			if (it == m_openQueries.end()) break;
			continue;
		}
	}
}

void FrameProfiler::ReportGPUEventStart(const std::string& eventIdentifier)
{
#ifdef _DEBUG
	for (const auto& openQuery : m_openQueries)
	{
		if (openQuery.name == eventIdentifier && openQuery.notEndedYet)
		{
			LOG_ERROR("Can't start query within a query with the same name!");
			return;
		}
	}
#endif

	m_openQueries.emplace_back();
	OpenQuery& newOpenQuery = m_openQueries.back();
	newOpenQuery.name = eventIdentifier;
	newOpenQuery.frame = static_cast<unsigned int>(m_recordedFrameDurations.size());
	newOpenQuery.start = 0;
	newOpenQuery.end = 0;
	newOpenQuery.notEndedYet = true;
	newOpenQuery.startQuery = GetQueryFromPool();
	GL_CALL(glQueryCounter, newOpenQuery.startQuery, GL_TIMESTAMP);
}

void FrameProfiler::ReportGPUEventEnd(const std::string& eventIdentifier)
{
	for (auto openQueryIt = m_openQueries.rbegin(); openQueryIt != m_openQueries.rend(); ++openQueryIt)
	{
		if (openQueryIt->name == eventIdentifier && openQueryIt->notEndedYet)
		{
			openQueryIt->notEndedYet = false;
			openQueryIt->endQuery = GetQueryFromPool();
			GL_CALL(glQueryCounter, openQueryIt->endQuery, GL_TIMESTAMP);
			return;
		}
	}

	LOG_ERROR("Query with identifier " << eventIdentifier << " was either already ended or never started!");
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
		GL_CALL(glCreateQueries, GL_TIMESTAMP, s_queryPoolAllocationSize, m_timerQueryPool.data());
	}

	gl::QueryId queryId = m_timerQueryPool.back();
	m_timerQueryPool.pop_back();
	return queryId;
}

void FrameProfiler::ReturnQueryToPool(gl::QueryId unusedQuery)
{
	m_timerQueryPool.push_back(unusedQuery);
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
		csvFile << m_recordedFrameDurations[frame] << ",";

		for (size_t evtTypeIdx = 0; evtTypeIdx < m_eventLists.size(); ++evtTypeIdx)
		{
			const auto& events = m_eventLists[evtTypeIdx].second;
			
			if (nextEvtIndex[evtTypeIdx] < events.size() && events[nextEvtIndex[evtTypeIdx]].frame == frame)
			{
				csvFile << events[nextEvtIndex[evtTypeIdx]].duration;
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