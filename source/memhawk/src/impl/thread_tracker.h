#pragma once


#include "alloc_info.h"
#include "i_stacktrace_tracker.h"
#include "lru_cache.h"
#include "stacktrace.h"

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <cstdint>

namespace memhawk
{

struct ThreadTracker
{
    absl::base_internal::SpinLock mt;

    uint32_t trackerId{};

    uint64_t totalAllocs{};
    uint64_t totalDeallocs{};
    TracedAllocSummary total{0};

    // Stacktrace -> StacktraceId
    LruCache<CompressedStacktrace, uint32_t> lruStacktraces;
    CompressedStacktrace localCompressed; // local element to reduce allocations

    // StacktraceId -> AllocSummary
    absl::flat_hash_map<uint32_t, AllocSummary> allocSummaries;

    // Tracker for all stacktraces
    IStacktraceTracker& btTracker;

    ThreadTracker(uint32_t id, uint32_t capacity, IStacktraceTracker& tracker)
        : trackerId(id), lruStacktraces(capacity), allocSummaries(capacity), btTracker(tracker)
    {
    }

    // Updates traceId in AllocInfo
    void SaveTraceId(AllocInfo& info, Stacktrace&& trace);

    void TrackAlloc(const AllocInfo& info);
    void TrackDealloc(const AllocInfo& info);

    absl::base_internal::SpinLockHolder AcquireLock();

    void Clear();
};

} // namespace memhawk
