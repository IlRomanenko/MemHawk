
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
#include <memory>
#include <mutex>

namespace memhawk
{

ABSL_CONST_INIT thread_local ThreadTracker* gtl_tracker = nullptr;
ABSL_CONST_INIT thread_local std::unique_ptr<MemHawk::ThreadTrackerFinalizer> gtl_trackerFinalizer;

constexpr const uint32_t ExitingThreadsTrackerId = 1U << 30;
constexpr const uint32_t InnerTrackerId = 1U << 31;

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
        const std::scoped_lock lock(m_mt);
        m_stopped = true;
        m_cv.notify_all();
    }
    LogInfo("Stopping memhawk tracker thread");
    if (m_worker.joinable())
    {
        m_worker.join();
    }
    LogInfo("Total trackers: " fSzt ", empty: " fSzt ", max postponed: " fSzt, m_thTrackers.size(),
            m_finishedTrackers.size(), m_maxPostponedSize);
    LogInfo("Inner traces: (" fSzt ", " fSzt "), external: " fSzt, m_innerBtTracker.StacktracesCount(),
            m_innerBtTracker.GetStorageSize(), m_btTracker.StacktracesCount());
    m_btTracker.Describe();

    LogInfo("ExternalTrackers");
    for (const auto& tracker : m_thTrackers)
    {
        tracker->LockTracker().PrintTracker();
    }
    LogInfo("ExitingThreadsTracker");
    m_exitingThreadsTracker->LockTracker().PrintTracker();
    LogInfo("InnerTracker");
    m_innerTracker->LockTracker().PrintTracker();
}

void MemHawk::PostponedConstruct()
{
    LogDebug("begin");
    const RecursionGuard<AllocTag> guard;
    {
        const RecursionGuard<InnerAllocTag> innerGuard;
        m_innerTracker = std::make_unique<ThreadTracker>(InnerTrackerId, *m_cfg.LruStackSize,
                                                         *m_cfg.CollapseRecursionDepth, m_innerBtTracker);
    }
    m_btTracker.PostponedConstruct();
    m_exitingThreadsTracker = std::make_unique<ThreadTracker>(ExitingThreadsTrackerId, *m_cfg.LruStackSize,
                                                              *m_cfg.CollapseRecursionDepth, m_btTracker);
    RegisterThread();
    if (*m_cfg.TrackingWorker)
    {
        m_workerStorage = std::make_unique<WorkerStorage>();
        m_workerStorage->writer =
            m_writersFactory->CreateWritersAdaptor(*m_cfg.Writers, std::make_shared<InnerStacktraceFinder>(*this));

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
    if (!m_finishedTrackers.empty())
    {
        const auto trackerId = m_finishedTrackers.back();
        m_finishedTrackers.pop_back();
        SetUpThreadTracker(m_thTrackers[trackerId].get());
        return;
    }
    const auto trackerId = m_thTrackers.size();
    m_thTrackers.emplace_back(
        std::make_unique<ThreadTracker>(trackerId, *m_cfg.LruStackSize, *m_cfg.CollapseRecursionDepth, m_btTracker));
    SetUpThreadTracker(m_thTrackers.back().get());
}

MemHawk::ThreadTrackerFinalizer::~ThreadTrackerFinalizer()
{
    // set special tracker (that will not be deallocated) for exiting thread
    gtl_tracker = m_memhawk->m_exitingThreadsTracker.get();

    // reclaim old tracker
    const absl::base_internal::SpinLockHolder lock(&m_memhawk->m_thTrackersMt);
    m_memhawk->m_finishedTrackers.push_back(m_tracker->GetTrackerId());
    LogDebug("Finished tracker: " fU32, m_tracker->GetTrackerId());
}

void MemHawk::SetUpThreadTracker(ThreadTracker* tracker)
{
    gtl_tracker = tracker;
    // setup non-trivial tracker finalizer
    gtl_trackerFinalizer = std::make_unique<ThreadTrackerFinalizer>(GuardTag<MemHawk>{}, this, gtl_tracker);
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

void MemHawk::PreFork()
{
    // manually lock mutex for tracking worker
    m_mt.lock();
    m_postponedMt.Lock();
}

void MemHawk::ParentPostFork()
{
    // not necessary to change state of thread trackers,
    // because all threads are preserved in parent process

    // safe, because was locked in PreFork
    m_postponedMt.Unlock();
    m_mt.unlock();
}

void MemHawk::ChildPostFork()
{
    // unlock all thread trackers first
    m_innerTracker->UnlockTrackerUnsafe();
    m_exitingThreadsTracker->UnlockTrackerUnsafe();
    for (auto& tracker : m_thTrackers)
    {
        tracker->UnlockTrackerUnsafe();
    }
    // safe, because was locked in PreFork
    m_postponedMt.Unlock();
    m_mt.unlock();
    // recreate condvar and inner thread. corresponding object will be destroyed in parent process, in child it's
    // necessary to reinit object otherwise there will be deadlock during MemHawk destruction
    new (&m_cv) std::condition_variable{};
    new (&m_worker) std::thread{};
    // set stopped as true because we can't track allocations in forked process
    m_stopped = true;
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

    const auto waitingDelay = std::chrono::milliseconds{*m_cfg.TrackerDumpingPeriodMs};
    auto nextTimepoint = std::chrono::steady_clock::now() + waitingDelay;
    while (!m_stopped)
    {
        std::unique_lock lock(m_mt);
        m_cv.wait_until(lock, nextTimepoint, [this]() { return m_stopped.load(); });
        nextTimepoint += waitingDelay;
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
    WorkerAccountThreadTracker(m_exitingThreadsTracker.get());
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
