#include "thread_tracker.h"

#include "alloc_info.h"
#include "logging.h"
#include "stacktrace.h"

namespace memhawk
{


ThreadTracker::LockedTracker ThreadTracker::LockTracker()
{
    return ThreadTracker::LockedTracker(*this);
}

void ThreadTracker::LockedTracker::PrintTracker()
{
    float cacheMissRatio = 0.0;
    if (m_tracker.m_cacheUsages != 0)
    {
        cacheMissRatio = static_cast<float>(m_tracker.m_cacheMisses) / static_cast<float>(m_tracker.m_cacheUsages);
    }
    LogInfo("TrackerId: " fU32 ", allocSummaries: (" fSzt "," fSzt "), lru: " fSzt
            ", cache: (miss-ratio: %.3f, misses: " fU64 ", usages: " fU64 "), allocs: " fU64 ", deallocs: " fU64,
            m_tracker.m_trackerId, m_tracker.m_allocSummaries.size(), m_tracker.m_allocSummaries.capacity(),
            m_tracker.m_lruStacktraces.Size(), cacheMissRatio, m_tracker.m_cacheMisses, m_tracker.m_cacheUsages,
            m_tracker.m_totalAllocs, m_tracker.m_totalDeallocs);
}

void ThreadTracker::LockedTracker::SaveTraceId(AllocInfo& info, Stacktrace& trace)
{
    m_tracker.m_cacheUsages++;
    trace.Compress(m_tracker.m_localCompressed);
    auto traceId = m_tracker.m_lruStacktraces.Touch(m_tracker.m_localCompressed);
    if (!traceId)
    {
        trace.CollapseRecursion(m_tracker.m_collapseDepth);
        m_tracker.m_cacheMisses++;
        traceId = m_tracker.m_btTracker.InsertStacktrace(trace);
        auto evicted = m_tracker.m_lruStacktraces.Insert(std::move(m_tracker.m_localCompressed), *traceId);
        if (evicted.has_value())
        {
            m_tracker.m_localCompressed = std::move(evicted).value();
        }
        else
        {
            m_tracker.m_localCompressed = {};
        }
    }
    info.traceId = *traceId;
}

void ThreadTracker::LockedTracker::TrackAlloc(const AllocInfo& info)
{
    m_tracker.m_totalAllocs++;

    auto summaryIt = m_tracker.m_allocSummaries.find(info.traceId);
    if (summaryIt == m_tracker.m_allocSummaries.end())
    {
        summaryIt = m_tracker.m_allocSummaries.insert({info.traceId, AllocSummary{}}).first;
    }
    auto& stats = summaryIt->second;
    stats += info;
    m_tracker.m_total += info;
}

void ThreadTracker::LockedTracker::TrackDealloc(const AllocInfo& info)
{
    m_tracker.m_totalDeallocs++;

    auto summaryIt = m_tracker.m_allocSummaries.find(info.traceId);
    if (summaryIt == m_tracker.m_allocSummaries.end())
    {
        summaryIt = m_tracker.m_allocSummaries.insert({info.traceId, AllocSummary{}}).first;
    }
    auto& stats = summaryIt->second;
    stats -= info;
    m_tracker.m_total -= info;
}

void ThreadTracker::LockedTracker::ConsumeDiff(SummariesMap& summaries, AllocSummary& total)
{
    m_tracker.m_allocSummaries.swap(summaries);
    m_tracker.m_allocSummaries.erase(m_tracker.m_allocSummaries.begin(), m_tracker.m_allocSummaries.end());
    total += m_tracker.m_total;
    m_tracker.m_total = {};
}

} // namespace memhawk
