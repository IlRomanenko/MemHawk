#pragma once

#include "alloc_info.h"
#include "stacktrace.h"
#include "stacktrace_tracker.h"
#include "stacktrace_tracker_static.h"
#include "thread_tracker.h"

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_set.h>
#include <absl/synchronization/mutex.h>
#include <boost/circular_buffer.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace memhawk
{

namespace bmi = boost::multi_index;

class MemHawk
{
public:
    explicit MemHawk(Config cfg);
    ~MemHawk();

    void PostponedConstruct();

    void TrackAlloc(AllocInfo& info, Stacktrace&& trace);
    void TrackDealloc(AllocInfo& info, const Stacktrace& trace);

private:
    struct WorkerData
    {
        std::ofstream summaryFile;
        std::ofstream stacktracesFile;
        absl::flat_hash_set<uint32_t> writtenStacktraces;
        AllocSummary summary;
        size_t updatedTraces{};

        absl::flat_hash_map<uint32_t, AllocSummary> localSummaries;

        struct IndexValue
        {
            uint32_t traceId{};
            mutable AllocSummary summary;

            explicit IndexValue(uint32_t id) : traceId{id}
            {
            }

            constexpr int64_t Size() const
            {
                return summary.size;
            }

            constexpr uint64_t TotalCount() const
            {
                return summary.totalCount;
            }
        };

        // clang-format off
        struct ByTraceId{};
        struct ByTotalSize{};
        struct ByTotalCount{};
        bmi::multi_index_container<
            IndexValue,
            bmi::indexed_by<
                bmi::hashed_unique<
                    bmi::tag<ByTraceId>,
                    bmi::member<IndexValue, uint32_t, &IndexValue::traceId>
                >,
                bmi::ordered_non_unique<
                    bmi::tag<ByTotalSize>,
                    bmi::const_mem_fun<IndexValue, int64_t, &IndexValue::Size>,
                    std::greater<>
                >,
                bmi::ordered_non_unique<
                    bmi::tag<ByTotalCount>,
                    bmi::const_mem_fun<IndexValue, uint64_t, &IndexValue::TotalCount>,
                    std::greater<>
                >
            >
        > index;
        // clang-format on
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
    void WorkerAccountThreadTracker(ThreadTracker& tracker);

    // Postponed allocs handlers
    void PostponeAlloc(const AllocInfo& info);
    void PostponeDealloc(const AllocInfo& info);
    void ProcessPostponed();

    // Threads registration
    void RegisterThread();
    void SetUpThreadFinishPromise(ThreadTracker* tracker);

private:
    Config m_cfg;

    // Inner worker
    std::mutex m_mt;
    std::condition_variable m_cv;
    std::atomic<bool> m_stopped{false};
    std::thread m_worker;
    std::unique_ptr<WorkerData> m_workerData;


    // Trackers
    absl::base_internal::SpinLock m_thTrackersMt;
    std::deque<std::unique_ptr<ThreadTracker>> m_thTrackers;
    std::deque<uint32_t> m_finishedTrackers;
    std::list<std::future<uint32_t>> m_finishPromises;

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
