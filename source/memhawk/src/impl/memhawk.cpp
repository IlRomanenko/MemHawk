
#include "memhawk.h"

#include "alloc_info.h"
#include "config.h"
#include "logging.h"
#include "macros.h"
#include "recursion_guard.h"
#include "scoped_sigmask.h"
#include "stacktrace.h"
#include "thread_tracker.h"
#include "trackers/stacktrace_tracker_static.h"
#include "writers/i_writer.h"

#include <absl/base/attributes.h>
#include <absl/cleanup/cleanup.h>
#include <absl/synchronization/mutex.h>
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <pthread.h>
#include <unistd.h>

namespace memhawk
{

alignas(64) ABSL_CONST_INIT thread_local ThreadTracker* gtl_tracker = nullptr;

MemHawk::MemHawk(MemHawkConfig cfg, std::unique_ptr<writers::IWritersFactory> factory)
    : m_cfg(std::move(cfg))
    , m_writersFactory{std::move(factory)}
    , m_btTracker(*m_cfg.ExternalTracker)
    , m_postponedCapacity(*m_cfg.MaxPostponed)
    , m_postponed(*m_cfg.MaxPostponed)
    , m_innerBtTracker(*m_cfg.InnerTracker)
{
    LogInfo("Start MemHawk()");
}

MemHawk::~MemHawk()
{
    if (!m_stopped)
    {
        Stop();
    }
}

void MemHawk::Stop()
{
    const RecursionGuard<AllocTag> guard;
    const RecursionGuard<InnerAllocTag> guardInner;
    {
        const std::lock_guard lock(m_mt);
        m_stopped = true;
        m_cv.notify_all();
    }
    if (m_worker.joinable())
    {
        m_worker.join();
    }
    gtl_tracker = nullptr;
    LogInfo("Total trackers: " fSzt ", empty: " fSzt ", max postponed: " fSzt, m_thTrackers.size(),
            m_finishedTrackers.size(), m_maxPostponedSize);
    LogInfo("Inner traces: (" fSzt ", " fSzt "), external: " fSzt, m_innerBtTracker.StacktracesCount(),
            m_innerBtTracker.GetStorageSize(), m_btTracker.StacktracesCount());

    for (const auto& tracker : m_thTrackers)
    {
        tracker->LockTracker().PrintTracker();
    }
    LogInfo("InnerTracker");
    m_innerTracker->LockTracker().PrintTracker();
}

void MemHawk::PostponedConstruct()
{
    LogDebug("begin");
    const RecursionGuard<AllocTag> guard;
    {
        const RecursionGuard<InnerAllocTag> innerGuard;
        m_innerTracker = std::make_unique<ThreadTracker>(m_thTrackers.size(), *m_cfg.LruStackSize,
                                                         *m_cfg.CollapseRecursionDepth, m_innerBtTracker);
    }
    m_btTracker.PostponedConstruct();
    RegisterThread();
    if (*m_cfg.TrackingWorker)
    {
        m_worker = std::thread([this]() { TrackingWorker(); });
    }
    LogDebug("end");
    LogInfo("MemHawk fully initialized");
}

void MemHawk::RegisterThread()
{
    if (unlikely(gtl_tracker != nullptr))
    {
        const auto trace = Stacktrace::Unwind(32, false).Describe();
        LogError("Trying to register already registered thread, stacktrace:\n" fStr, trace.c_str());
        return;
    }
    const RecursionGuard<AllocTag> guard;

    const absl::base_internal::SpinLockHolder lock(&m_thTrackersMt);
    for (auto it = m_finishPromises.begin(); it != m_finishPromises.end();)
    {
        if (it->wait_for(std::chrono::seconds{0}) == std::future_status::ready)
        {
            const auto trackerId = it->get();
            LogDebug("Finished tracker: " fU32, trackerId);
            m_finishedTrackers.push_back(trackerId);
            it = m_finishPromises.erase(it);
        }
        else
        {
            it++;
        }
    }
    if (!m_finishedTrackers.empty())
    {
        const auto trackerId = m_finishedTrackers.back();
        m_finishedTrackers.pop_back();
        gtl_tracker = m_thTrackers[trackerId].get();
        SetUpThreadFinishPromise(trackerId);
        return;
    }
    const auto trackerId = m_thTrackers.size();
    m_thTrackers.emplace_back(
        std::make_unique<ThreadTracker>(trackerId, *m_cfg.LruStackSize, *m_cfg.CollapseRecursionDepth, m_btTracker));
    gtl_tracker = m_thTrackers.back().get();
    SetUpThreadFinishPromise(trackerId);
}

void MemHawk::SetUpThreadFinishPromise(uint32_t trackerId)
{
    std::promise<uint32_t> exitPromise;
    exitPromise.set_value_at_thread_exit(trackerId);
    m_finishPromises.push_back(exitPromise.get_future());
}

void MemHawk::TrackAlloc(AllocInfo& info, Stacktrace& trace, bool isExternal)
{
    if (likely(isExternal))
    {
        // external allocation
        if (unlikely(gtl_tracker == nullptr))
        {
            RegisterThread();
        }
        auto lockedTracker = gtl_tracker->LockTracker();
        lockedTracker.SaveTraceId(info, trace);
        lockedTracker.TrackAlloc(info);
    }
    else
    {
        // internal allocation of memhawk
        const RecursionGuard<InnerAllocTag> innerGuard;
        // set trace id manually, otherwise there can be malloc recursion upon inserting into tracker caches
        info.traceId = m_innerBtTracker.InsertStacktrace(trace);

        if (innerGuard)
        {
            ProcessPostponed();

            auto lockedTracker = m_innerTracker->LockTracker();
            lockedTracker.TrackAlloc(info);
        }
        else
        {
            PostponeAlloc(info);
        }
    }
}

void MemHawk::TrackDealloc(AllocInfo& info, bool isExternal)
{
    const RecursionGuard<AllocTag> guard;
    if (likely(isExternal))
    {
        // external deallocation
        if (unlikely(gtl_tracker == nullptr))
        {
            RegisterThread();
        }
        auto lockedTracker = gtl_tracker->LockTracker();
        lockedTracker.TrackDealloc(info);
    }
    else
    {
        // internal deallocation of memhawk
        const RecursionGuard<InnerAllocTag> innerGuard;
        if (innerGuard)
        {
            ProcessPostponed();
            auto lockedTracker = m_innerTracker->LockTracker();
            lockedTracker.TrackDealloc(info);
        }
        else
        {
            PostponeDealloc(info);
        }
    }
}

void MemHawk::InvalidateModulesCache()
{
    m_modulesCacheInvalidated.store(true);
}

void MemHawk::ProcessPostponed()
{
    do
    {
        Postponed delayed{};
        {
            const absl::base_internal::SpinLockHolder lock(&m_postponedMt);
            if (m_postponed.empty())
            {
                return;
            }
            delayed = std::move(m_postponed.front());
            m_postponed.pop_front();
        }
        LogDebug("processing: [size: " fU32 ", op: " fI32 "]", delayed.info.size, static_cast<int>(delayed.op));

        auto lockedTracker = m_innerTracker->LockTracker();
        if (delayed.op == Postponed::Operation::Alloc)
        {
            lockedTracker.TrackAlloc(delayed.info);
        }
        else
        {
            lockedTracker.TrackDealloc(delayed.info);
        }
    } while (true);
}

void MemHawk::PostponeAlloc(const AllocInfo& info)
{
    const absl::base_internal::SpinLockHolder lock(&m_postponedMt);
    if (m_postponed.size() >= m_postponedCapacity)
    {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .op = Postponed::Operation::Alloc};
    m_postponed.push_back(std::move(op));
    m_maxPostponedSize = std::max(m_maxPostponedSize, m_postponed.size());
}

void MemHawk::PostponeDealloc(const AllocInfo& info)
{
    const absl::base_internal::SpinLockHolder lock(&m_postponedMt);
    if (m_postponed.size() >= m_postponedCapacity)
    {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .op = Postponed::Operation::Free};
    m_postponed.push_back(std::move(op));
    m_maxPostponedSize = std::max(m_maxPostponedSize, m_postponed.size());
}

void MemHawk::TrackingWorker()
{
    // set guard, that indicates, that this thread is local to memhawk
    const RecursionGuard<AllocTag> guard;
    pthread_setname_np(pthread_self(), "MemHawkTh");
    const ScopedSignalBlocker signalBlocker{};

    LogInfo("Tracking worker started");

    m_workerStorage = std::make_unique<WorkerStorage>();
    m_workerStorage->writer =
        m_writersFactory->CreateWritersAdaptor(*m_cfg.Writers, std::make_shared<InnerStacktraceFinder>(*this));

    while (!m_stopped)
    {
        {
            std::unique_lock lock(m_mt);
            m_cv.wait_for(lock, std::chrono::milliseconds{*m_cfg.TrackerDumpingPeriodMs},
                          [this]() { return m_stopped.load(); });
        }
        WorkerUpdateData();
        WorkerPrintData();
    }

    LogInfo("Tracking worker finished");
}

void MemHawk::WorkerUpdateData()
{
    const absl::base_internal::SpinLockHolder lock(&m_thTrackersMt);

    for (const auto& tracker : m_thTrackers)
    {
        WorkerAccountThreadTracker(tracker.get());
    }
    WorkerAccountThreadTracker(m_innerTracker.get());
    if (m_modulesCacheInvalidated.exchange(false, std::memory_order_relaxed))
    {
        m_workerStorage->writer->UpdateModules();
    }
}

void MemHawk::WorkerAccountThreadTracker(ThreadTracker* tracker)
{
    {
        auto lockedTracker = tracker->LockTracker();
        lockedTracker.ConsumeDiff(m_workerStorage->localSummaries, m_workerStorage->summary);
    }
    m_workerStorage->writer->AccountSnapshot(m_workerStorage->localSummaries, m_workerStorage->summary);
    m_workerStorage->summary = {};
    m_workerStorage->localSummaries.clear();
}

void MemHawk::WorkerPrintData()
{
    m_workerStorage->writer->FlushData();
}

} // namespace memhawk
