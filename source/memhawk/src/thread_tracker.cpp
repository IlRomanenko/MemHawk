#include "thread_tracker.h"

#include "macros.h"

void InThreadTracker::TrackAlloc(const AllocInfo& info, Stacktrace&& trace)
{
    absl::MutexLock lock(&mt);
    totalAllocs++;

    auto traceIt = knownStacktraces.find(info.traceHash);
    if (traceIt == knownStacktraces.end()) {
        btTracker.SaveStacktrace(std::move(trace));
        traceIt = knownStacktraces.insert({info.traceHash, allocsStats.size()}).first;
        allocsStats.emplace_back(TracedAllocSummary{info.traceHash});
    }
    auto statsId = traceIt->second;
    auto& stats = allocsStats[statsId];
    if (unlikely(!stats.changed)) {
        changedAllocsStats.emplace_back(statsId);
    }
    stats += info;
    total += info;
}

void InThreadTracker::TrackDealloc(const AllocInfo& info)
{
    absl::MutexLock lock(&mt);
    totalDeallocs++;

    auto traceIt = knownStacktraces.find(info.traceHash);
    if (traceIt == knownStacktraces.end()) {
        traceIt = knownStacktraces.insert({info.traceHash, allocsStats.size()}).first;
        allocsStats.emplace_back(TracedAllocSummary{info.traceHash});
    }
    auto statsId = traceIt->second;
    auto& stats = allocsStats[statsId];
    if (unlikely(!stats.changed)) {
        changedAllocsStats.emplace_back(statsId);
    }
    stats -= info;
    total -= info;
}
