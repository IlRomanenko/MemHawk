#pragma once

#include "alloc_info.h"
#include "config.h"
#include "i_stacktrace_tracker.h"
#include "stacktrace.h"
#include "thread_tracker.h"
#include "trackers/stacktrace_tracker.h"
#include "trackers/stacktrace_tracker_static.h"
#include "writers/i_writer.h"

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_set.h>
#include <absl/synchronization/mutex.h>
#include <boost/circular_buffer.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace memhawk
{

class MemHawk
{
public:
    explicit MemHawk(MemHawkConfig cfg, std::unique_ptr<writers::IWritersFactory> factory);
    ~MemHawk();

    void Stop();

    void PostponedConstruct();
    void InvalidateModulesCache();

    void TrackAlloc(AllocInfo& info, Stacktrace& trace, bool isExternal);
    void TrackDealloc(AllocInfo& info, bool isExternal);


    void PreFork();
    void ParentPostFork();
    void ChildPostFork();

public:
    class ThreadTrackerFinalizer
    {
    public:
        explicit ThreadTrackerFinalizer(GuardTag<MemHawk>, MemHawk* memhawk, ThreadTracker* tracker)
            : m_memhawk{memhawk}, m_tracker{tracker}
        {
        }

        ThreadTrackerFinalizer() = delete;
        ~ThreadTrackerFinalizer();

    private:
        friend MemHawk;

    private:
        MemHawk* m_memhawk;
        ThreadTracker* m_tracker{};
    };

private:
    class InnerStacktraceFinder : public IStacktraceFinder
    {
    public:
        explicit InnerStacktraceFinder(MemHawk& memhawk) : m_memhawk(memhawk)
        {
        }

        InnerStacktraceFinder(const InnerStacktraceFinder&) = delete;
        InnerStacktraceFinder& operator=(const InnerStacktraceFinder&) = delete;

        std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override
        {
            auto trace = m_memhawk.m_btTracker.GetStacktraceFromId(traceId);
            if (!trace)
            {
                trace = m_memhawk.m_innerBtTracker.GetStacktraceFromId(traceId);
            }
            return trace;
        }

    private:
        MemHawk& m_memhawk;
    };

    struct WorkerStorage
    {
        std::unique_ptr<writers::IWriterStrategy> writer;
        SummariesMap localSummaries;
        AllocSummary summary;
    };

    struct Postponed
    {
        enum class Operation
        {
            Alloc,
            Free
        };
        AllocInfo info{0, 0};
        Operation op = Operation::Alloc;
    };

    // Tracking thread
    void TrackingWorker();
    void WorkerUpdateData();
    void WorkerPrintData();
    void WorkerAccountThreadTracker(ThreadTracker* tracker);

    // Postponed allocs handlers
    void PostponeAlloc(const AllocInfo& info);
    void PostponeDealloc(const AllocInfo& info);
    void ProcessPostponed();

    // Threads registration
    void RegisterThread();
    void SetUpThreadTracker(ThreadTracker* tracker);

private:
    MemHawkConfig m_cfg;

    std::atomic_bool m_modulesCacheInvalidated{true};

    // Inner worker
    std::mutex m_mt;
    std::condition_variable m_cv;
    std::atomic<bool> m_stopped{false};
    std::thread m_worker;
    std::unique_ptr<writers::IWritersFactory> m_writersFactory;
    std::unique_ptr<WorkerStorage> m_workerStorage;

    // Trackers
    absl::base_internal::SpinLock m_thTrackersMt;
    std::deque<std::unique_ptr<ThreadTracker>> m_thTrackers;
    std::deque<uint32_t> m_finishedTrackers;
    // special tracker for threads, that are currently terminating and started to deallocate tls objects
    // can be modified concurrently by multiple threads but overall amount of operations is negligible
    // and shouldn't harm performance
    std::unique_ptr<ThreadTracker> m_exitingThreadsTracker;

    StacktraceTracker m_btTracker;

    // Postponed and inner tracking
    absl::base_internal::SpinLock m_postponedMt;
    size_t m_postponedCapacity{};
    size_t m_maxPostponedSize{};
    // fixed size buffer for postponed operations
    boost::circular_buffer<Postponed> m_postponed;
    // tracker for all inner allocations
    std::unique_ptr<ThreadTracker> m_innerTracker;
    StaticStacktraceTracker m_innerBtTracker;
};

} // namespace memhawk
