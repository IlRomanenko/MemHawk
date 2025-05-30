#include "thread_tracker.h"

#include "alloc_info.h"
#include "stacktrace.h"

#include <absl/base/internal/spinlock.h>

namespace memhawk
{


absl::base_internal::SpinLockHolder ThreadTracker::AcquireLock()
{
    return absl::base_internal::SpinLockHolder(&mt);
}

void ThreadTracker::SaveTraceId(AllocInfo& info, Stacktrace&& trace)
{
    trace.Compress(localCompressed);
    auto traceId = lruStacktraces.Touch(localCompressed);
    if (!traceId)
    {
        traceId = btTracker.InsertStacktrace(std::move(trace));
        auto evicted = lruStacktraces.Insert(std::move(localCompressed), *traceId);
        if (evicted.has_value())
        {
            localCompressed = std::move(evicted).value();
        }
        else
        {
            localCompressed = {};
        }
    }
    info.traceId = *traceId;
}

void ThreadTracker::TrackAlloc(const AllocInfo& info)
{
    totalAllocs++;

    auto summaryIt = allocSummaries.find(info.traceId);
    if (summaryIt == allocSummaries.end())
    {
        summaryIt = allocSummaries.insert({info.traceId, AllocSummary{}}).first;
    }
    auto& stats = summaryIt->second;
    stats += info;
    total += info;
}

void ThreadTracker::TrackDealloc(const AllocInfo& info)
{
    totalDeallocs++;

    auto summaryIt = allocSummaries.find(info.traceId);
    if (summaryIt == allocSummaries.end())
    {
        summaryIt = allocSummaries.insert({info.traceId, AllocSummary{}}).first;
    }
    auto& stats = summaryIt->second;
    stats -= info;
    total -= info;
}

void ThreadTracker::Clear()
{
    // allocSummaries.erase(allocSummaries.begin(), allocSummaries.end());
    allocSummaries.clear();
}

} // namespace memhawk
