#pragma once


#include "alloc_info.h"
#include "stacktrace_tracker.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <cstdint>
#include <vector>

struct InThreadTracker
{
    // works as spinlock in fast path
    absl::Mutex mt;

    uint32_t trackerId{};

    uint64_t totalAllocs{};
    uint64_t totalDeallocs{};
    TracedAllocSummary total{0};

    // Stacktrace.Hash() -> TracedAllocSummary index
    absl::flat_hash_map<uint32_t, uint32_t> knownStacktraces;

    std::vector<TracedAllocSummary> allocsStats;
    std::vector<uint32_t> changedAllocsStats;

    StacktraceTracker& btTracker;

    InThreadTracker(uint32_t id, StacktraceTracker& tracker) : trackerId(id), btTracker(tracker)
    {
    }

    void TrackAlloc(const AllocInfo& info, Stacktrace&& trace);
    void TrackDealloc(const AllocInfo& info);
};
