#pragma once


#include "alloc_info.h"
#include "i_stacktrace_tracker.h"
#include "lru_cache.h"
#include "spinlock.h"
#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <cstdint>
#include <mutex>

namespace memhawk
{

using SummariesMap = absl::flat_hash_map<uint32_t, AllocSummary>;

class ThreadTracker
{
public:
    class LockedTracker
    {
    public:
        // Updates traceId in AllocInfo
        void SaveTraceId(AllocInfo& info, Stacktrace&& trace);

        void TrackAlloc(const AllocInfo& info);
        void TrackDealloc(const AllocInfo& info);

        void ConsumeDiff(SummariesMap& summaries, AllocSummary& total);

        void PrintTracker();

    private:
        friend class ThreadTracker;

        explicit LockedTracker(ThreadTracker& tracker) : m_tracker(tracker), m_lock(m_tracker.m_mt)
        {
        }

        ThreadTracker& m_tracker;
        std::lock_guard<SpinLock> m_lock;
    };

public:
    ThreadTracker(uint32_t id, uint32_t capacity, uint32_t collapseDepth, IStacktraceTracker& tracker)
        : m_trackerId(id)
        , m_collapseDepth(collapseDepth)
        , m_lruStacktraces(capacity)
        , m_allocSummaries(capacity)
        , m_btTracker(tracker)
    {
    }

    LockedTracker LockTracker();

    uint32_t GetTrackerId() const
    {
        return m_trackerId;
    }

private:
    alignas(64) SpinLock m_mt;

    uint32_t m_trackerId{};
    uint32_t m_collapseDepth{};

    uint64_t m_totalAllocs{};
    uint64_t m_totalDeallocs{};

    uint64_t m_cacheMisses{};
    uint64_t m_cacheUsages{};

    AllocSummary m_total;

    // Stacktrace -> StacktraceId
    LruCache<CompressedStacktrace, uint32_t> m_lruStacktraces;
    CompressedStacktrace m_localCompressed; // local element to reduce allocations

    // StacktraceId -> AllocSummary
    SummariesMap m_allocSummaries;

    // Tracker for all stacktraces
    IStacktraceTracker& m_btTracker;
};

} // namespace memhawk
