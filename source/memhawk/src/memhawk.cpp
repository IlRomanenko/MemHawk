
#include "memhawk.h"

#include "alloc_info.h"
#include "global_storage.h"
#include "log.h"
#include "log_name.h"
#include "macros.h"
#include "recursion_guard.h"
#include "stacktrace.h"
#include "thread_tracker.h"

#include <absl/cleanup/cleanup.h>
#include <absl/synchronization/mutex.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <boost/range/iterator_range.hpp>
#include <fmt/format.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <unistd.h>

struct OnThreadFinishGuard
{
    ~OnThreadFinishGuard();
};

thread_local bool gtl_innerTracking = true;
thread_local InThreadTracker* gtl_tracker = nullptr;
thread_local std::unique_ptr<OnThreadFinishGuard> gtl_onThreadFinish;
thread_local void* gtl_allocRetPtr[16]; // should be no more, than 5
thread_local uint32_t gtl_maxRetPtrLevel = 0;
struct AllocRetPtrTag;

constexpr size_t MaxPostponed = 256;

OnThreadFinishGuard::~OnThreadFinishGuard()
{
    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        storage->GetMemHawk()->OnThreadFinish(gtl_tracker);
    }
}

MemHawk::MemHawk() : m_postponedCapacity(MaxPostponed), m_postponed(MaxPostponed)
{
    LogInfo("Start MemHawk()");
}

// unused, will be used in future
void PrintTracker(const std::unique_ptr<InThreadTracker>& tracker)
{
    std::stringstream str;
    std::vector<TracedAllocSummary> pv;

    str << "\n";
    str << fmt::format("allocs: {}, deallocs: {}\n", tracker->totalAllocs, tracker->totalDeallocs);
    str << fmt::format("summary: {:.3f} mb, active: {}\n", tracker->total.summary.size / 1024.0 / 1024,
                       tracker->total.summary.count);
    for (const auto& alloc : tracker->allocsStats) {
        if (alloc.summary.count == 0) {
            continue;
        }
        pv.push_back(alloc);
    }
    std::sort(pv.begin(), pv.end(), [](const auto& lhs, const auto& rhs) { return lhs.traceHash < rhs.traceHash; });
    for (const auto& alloc : pv) {
        str << fmt::format("[{}: {} :: hash: {}]", alloc.summary.count, alloc.summary.size, alloc.traceHash);
    }
    LogInfo(fStr, str.str().c_str());
}

MemHawk::~MemHawk()
{
    RecursionGuard<AllocTag> guard;
    RecursionGuard<InnerAllocTag> guardInner;
    gtl_innerTracking = false;
    {
        std::lock_guard lock(m_mt);
        m_stopped = true;
        m_cv.notify_all();
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }
    LogInfo("Total trackers: " fSzt ", empty: " fSzt ", max postponed: " fSzt ", max ret ptr level: " fU32,
            m_thTrackers.size(), m_finishedTrackers.size(), m_maxPostponedSize, m_maxRetPtrIndex);
    LogInfo("Inner traces: " fSzt ", external: " fSzt, m_innerBtTracker.StacktracesCount(),
            m_btTracker.StacktracesCount());
}

void MemHawk::PostponedConstruct()
{
    LogDebug("begin");
    RecursionGuard<AllocTag> guard;
    {
        RecursionGuard<InnerAllocTag> innerGuard;
        m_innerBtTracker.PostponedConstruct();
        m_innerTracker = std::make_unique<InThreadTracker>(m_thTrackers.size(), m_innerBtTracker);
    }
    m_btTracker.PostponedConstruct();

    m_worker = std::thread([this]() { TrackingWorker(); });
    LogDebug("end");

    LogInfo("MemHawk fully initialized");
}

void MemHawk::RegisterThread()
{
    if (unlikely(gtl_tracker != nullptr)) {
        const auto trace = Stacktrace::Unwind(32).Describe();
        LogError("Trying to register already registered thread, stacktrace:\n" fStr, trace.c_str());
        return;
    }
    RecursionGuard<AllocTag> guard;
    gtl_onThreadFinish = std::make_unique<OnThreadFinishGuard>();
    absl::MutexLock lock(&m_thTrackersMt);
    if (!m_finishedTrackers.empty()) {
        auto trackerId = m_finishedTrackers.back();
        m_finishedTrackers.pop_back();
        gtl_tracker = m_thTrackers[trackerId].get();
        return;
    }
    m_thTrackers.emplace_back(std::make_unique<InThreadTracker>(m_thTrackers.size(), m_btTracker));
    gtl_tracker = m_thTrackers.back().get();
}

void MemHawk::OnThreadFinish(InThreadTracker* tracker)
{
    RecursionGuard<AllocTag> guard;
    absl::MutexLock lock(&m_thTrackersMt);
    m_finishedTrackers.emplace_back(tracker->trackerId);
    m_maxRetPtrIndex = std::max(m_maxRetPtrIndex, gtl_maxRetPtrLevel);
}

void MemHawk::TrackAlloc(AllocInfo& info, Stacktrace&& trace)
{
    RecursionGuard<AllocRetPtrTag> retPtrGuard;
    {
        auto span = trace.GetTrace();
        if (likely(span.size() > 1)) {
            gtl_allocRetPtr[retPtrGuard.Level()] = span[1];
        } else {
            gtl_allocRetPtr[retPtrGuard.Level()] = nullptr;
        }
        gtl_maxRetPtrLevel = std::max(gtl_maxRetPtrLevel, retPtrGuard.Level());
    }

    RecursionGuard<AllocTag> guard;
    if (guard) {
        if (unlikely(gtl_tracker == nullptr)) {
            RegisterThread();
        }
        gtl_tracker->TrackAlloc(info, std::move(trace));
    } else if (gtl_innerTracking) {
        RecursionGuard<InnerAllocTag> innerGuard;

        // shrink and update hash, track only inner stacktrace in order to reduce index size
        trace.ShrinkByPtr(gtl_allocRetPtr[retPtrGuard.Level() - 1]);
        info.traceHash = trace.Hash();

        if (innerGuard) {
            ProcessPostponed();
            m_innerTracker->TrackAlloc(info, std::move(trace));
        } else {
            PostponeAlloc(info, std::move(trace));
        }
    }
}

void MemHawk::TrackDealloc(const AllocInfo& info, const Stacktrace& trace)
{
    RecursionGuard<AllocRetPtrTag> retPtrGuard;
    {
        auto span = trace.GetTrace();
        if (likely(span.size() > 1)) {
            gtl_allocRetPtr[retPtrGuard.Level()] = span[1];
        } else {
            gtl_allocRetPtr[retPtrGuard.Level()] = nullptr;
        }
        gtl_maxRetPtrLevel = std::max(gtl_maxRetPtrLevel, retPtrGuard.Level());
    }

    RecursionGuard<AllocTag> guard;
    if (guard) {
        if (unlikely(gtl_tracker == nullptr)) {
            RegisterThread();
        }
        gtl_tracker->TrackDealloc(info);
    } else if (gtl_innerTracking) {
        RecursionGuard<InnerAllocTag> innerGuard;
        if (innerGuard) {
            ProcessPostponed();
            m_innerTracker->TrackDealloc(info);
        } else {
            PostponeDealloc(info);
        }
    }
}

void MemHawk::ProcessPostponed()
{
    do {
        Postponed delayed{};
        {
            absl::MutexLock lock(&m_postponedMt);
            if (m_postponed.empty()) {
                return;
            }
            delayed = std::move(m_postponed.front());
            m_postponed.pop_front();
        }
        LogDebug("processing: [size: " fU32 ", op: " fI32 "]", delayed.info.traceHash, static_cast<int>(delayed.op));
        if (delayed.op == Postponed::Operation::Alloc) {
            m_innerTracker->TrackAlloc(delayed.info, std::move(delayed.trace));
        } else {
            m_innerTracker->TrackDealloc(delayed.info);
        }
    } while (true);
}

void MemHawk::PostponeAlloc(const AllocInfo& info, Stacktrace&& trace)
{
    if (m_postponed.size() >= m_postponedCapacity) {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .trace = std::move(trace), .op = Postponed::Operation::Alloc};
    m_postponed.push_back(std::move(op));
    m_maxPostponedSize = std::max(m_maxPostponedSize, m_postponed.size());
}

void MemHawk::PostponeDealloc(const AllocInfo& info)
{
    if (m_postponed.size() >= m_postponedCapacity) {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .trace = {}, .op = Postponed::Operation::Free};
    m_postponed.push_back(std::move(op));
    m_maxPostponedSize = std::max(m_maxPostponedSize, m_postponed.size());
}

void MemHawk::TrackingWorker()
{
    RegisterThread();
    pthread_setname_np(pthread_self(), "MemHawkTh");
    sigset_t mask{};
    sigfillset(&mask); // Block all signals
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    LogInfo("Tracking worker started");

    m_workerData = std::make_unique<WorkerData>();

    m_workerData->summaryFile = std::ofstream(GetProcessLogName("summary"), std::ios_base::out | std::ios_base::trunc);
    m_workerData->stacktracesFile =
        std::ofstream(GetProcessLogName("stacktraces"), std::ios_base::out | std::ios_base::trunc);

    while (!m_stopped) {
        {
            std::unique_lock lock(m_mt);
            m_cv.wait_for(lock, std::chrono::seconds{1}, [this]() { return !!m_stopped; });
        }
        WorkerUpdateData();
        WorkerPrintData();
    }
    m_workerData->summaryFile.close();
    m_workerData->stacktracesFile.close();

    LogInfo("Tracking worker finished");
}

void MemHawk::WorkerUpdateData()
{
    absl::MutexLock lock(&m_thTrackersMt);
    m_workerData->updatedTraces = 0;

    for (const auto& tracker : m_thTrackers) {
        // gtl_tracker has to be registered at that moment
        if (tracker->trackerId == gtl_tracker->trackerId) {
            // Will handle this case separately
            continue;
        }
        WorkerAccountThreadTracker(*tracker);
    }
    // todo: account inner trackers
    WorkerAccountInnerTracker(*m_innerTracker);
    WorkerAccountInnerTracker(*gtl_tracker);
}

void MemHawk::WorkerAccountThreadTracker(InThreadTracker& tracker)
{
    auto& byTraceIdIndex = m_workerData->index.get<WorkerData::ByTraceId>();
    absl::MutexLock trackerLock(&tracker.mt);
    for (const auto& allocIndex : tracker.changedAllocsStats) {
        auto& trackedSummary = tracker.allocsStats[allocIndex];
        auto statIt = byTraceIdIndex.find(trackedSummary.traceHash);
        if (statIt == byTraceIdIndex.end()) {
            statIt = byTraceIdIndex.insert(WorkerData::IndexValue{trackedSummary.traceHash}).first;
        }
        byTraceIdIndex.modify(statIt, [&trackedSummary](WorkerData::IndexValue& value) {
            value.summary += trackedSummary.ConsumeDiff();
        });
    }
    // ConsumeDiff should be the last call and be called once
    m_workerData->summary += tracker.total.ConsumeDiff();
    m_workerData->updatedTraces += tracker.changedAllocsStats.size();
    tracker.changedAllocsStats.clear();
}

void MemHawk::WorkerAccountInnerTracker(InThreadTracker& tracker)
{
    absl::MutexLock trackerLock(&tracker.mt);
    m_workerData->summary += tracker.total.ConsumeDiff();
}

void MemHawk::WorkerPrintData()
{
    absl::flat_hash_set<uint32_t> newStacktraces;

    const auto& bySizeIndex = m_workerData->index.get<WorkerData::ByTotalSize>();
    size_t topElementsCount = std::min(10ul, bySizeIndex.size());
    const auto bySizeRange = boost::make_iterator_range_n(bySizeIndex.begin(), topElementsCount);

    std::stringstream str;
    str << absl::FormatTime(absl::Now()) << "\n";
    str << fmt::format("Application heap: {:.3f}mb, active: {}, total: {}, memhawk overhead: {:.3f}mb\n",
                       m_workerData->summary.size / 1024.0 / 1024, m_workerData->summary.count,
                       m_workerData->summary.total, m_workerData->summary.overhead / 1024.0 / 1024);

    for (const auto& value : bySizeRange) {
        if (value.summary.count == 0) {
            continue;
        }
        const auto it = m_workerData->writtenStacktraces.insert(value.traceId);
        if (it.second) {
            newStacktraces.insert(value.traceId);
        }
        const auto average = value.summary.count == 0 ? 0.0 : 1.0 * value.summary.size / value.summary.count;
        str << fmt::format("TraceId: {}, active: {}, size: {:.3f}mb, average: {:.3f}b, total: {}\n", value.traceId,
                           value.summary.count, value.summary.size / 1024.0 / 1024, average, value.summary.total);
    }
    str << "\n\n";
    m_workerData->summaryFile << str.str();

    for (const auto& traceId : newStacktraces) {
        auto trace = m_btTracker.GetStacktraceFromHash(traceId);
        if (unlikely(!trace.has_value())) {
            trace = m_innerBtTracker.GetStacktraceFromHash(traceId);
        }

        if (!trace.has_value()) {
            LogWarning("Missed stacktrace: " fU32, traceId);
            continue;
        }

        auto traceStr = trace.value().Describe();
        m_workerData->stacktracesFile << fmt::format("TraceId: {}\n{}\n", traceId, traceStr) << std::endl;
    }
    m_workerData->stacktracesFile.flush();
    m_workerData->summaryFile.flush();
}
