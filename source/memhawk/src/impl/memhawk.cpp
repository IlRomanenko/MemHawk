
#include "memhawk.h"

#include "alloc_info.h"
#include "config.h"
#include "log.h"
#include "log_name.h"
#include "macros.h"
#include "recursion_guard.h"
#include "stacktrace.h"
#include "stacktrace_tracker_fixed_size.h"
#include "thread_tracker.h"

#include <absl/base/attributes.h>
#include <absl/cleanup/cleanup.h>
#include <absl/synchronization/mutex.h>
#include <boost/range/iterator_range.hpp>
#include <fmt/format.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <future>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <unistd.h>

namespace memhawk
{

thread_local ThreadTracker* gtl_tracker = nullptr;
ABSL_CONST_INIT thread_local void* gtl_retPtrs[8] = {
    nullptr,
}; // only first element

struct RetPtrTag
{
};

MemHawk::MemHawk() : m_postponedCapacity(gl_config.MaxPostponed), m_postponed(gl_config.MaxPostponed)
{
    LogInfo("Start MemHawk()");
}

void PrintTracker(ThreadTracker* tracker)
{
    LogInfo("TrackerId: " fU32 ", allocSummaries: (" fSzt "," fSzt "), lru: " fSzt, tracker->trackerId,
            tracker->allocSummaries.size(), tracker->allocSummaries.capacity(), tracker->lruStacktraces.Size());
}

MemHawk::~MemHawk()
{
    RecursionGuard<AllocTag> guard;
    RecursionGuard<InnerAllocTag> guardInner;
    {
        std::lock_guard lock(m_mt);
        m_stopped = true;
        m_cv.notify_all();
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }
    LogInfo("Total trackers: " fSzt ", empty: " fSzt ", max postponed: " fSzt, m_thTrackers.size(),
            m_finishedTrackers.size(), m_maxPostponedSize);
    LogInfo("Inner traces: " fSzt ", external: " fSzt, m_innerBtTracker.StacktracesCount(),
            m_btTracker.StacktracesCount());

    for (const auto& tracker : m_thTrackers) {
        PrintTracker(tracker.get());
    }
    LogInfo("InnerTracker");
    PrintTracker(m_innerTracker.get());
    LogInfo("WorkerData.index");
    for (const auto& elem : m_workerData->index) {
        if (elem.summary.active == 0) {
            continue;
        }
        if (!IsFixedTrackerId(elem.traceId)) {
            continue;
        }
        LogInfo("TraceId: " fU32 ", active: " fI64 ", size: " fI64 ", overhead: " fI64 ", total: " fI64, elem.traceId,
                elem.summary.active, elem.summary.size, elem.summary.overhead, elem.summary.total);
    }
}

void MemHawk::PostponedConstruct()
{
    LogDebug("begin");
    RecursionGuard<AllocTag> guard;
    {
        RecursionGuard<InnerAllocTag> innerGuard;
        m_innerTracker = std::make_unique<ThreadTracker>(m_thTrackers.size(), gl_config.LruStackSize, m_innerBtTracker);
    }
    m_btTracker.PostponedConstruct();
    m_workerData = std::make_unique<WorkerData>();
    RegisterThread();
    if (gl_config.StartTrackingWorker) {
        m_worker = std::thread([this]() { TrackingWorker(); });
    }
    LogDebug("end");
    LogInfo("MemHawk fully initialized");
}

void MemHawk::RegisterThread()
{
    if (unlikely(gtl_tracker != nullptr)) {
        const auto trace = Stacktrace::Unwind(32, 0).Describe();
        LogError("Trying to register already registered thread, stacktrace:\n" fStr, trace.c_str());
        return;
    }
    RecursionGuard<AllocTag> guard;

    absl::MutexLock lock(&m_thTrackersMt);
    for (auto it = m_finishPromises.begin(); it != m_finishPromises.end();) {
        if (it->wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            const auto trackerId = it->get();
            LogDebug("Finished tracker: " fU32, trackerId);
            m_finishedTrackers.push_back(trackerId);
            it = m_finishPromises.erase(it);
        } else {
            it++;
        }
    }
    if (!m_finishedTrackers.empty()) {
        auto trackerId = m_finishedTrackers.back();
        m_finishedTrackers.pop_back();
        gtl_tracker = m_thTrackers[trackerId].get();
        SetUpThreadFinishPromise(gtl_tracker);
        return;
    }
    m_thTrackers.emplace_back(
        std::make_unique<ThreadTracker>(m_thTrackers.size(), gl_config.LruStackSize, m_btTracker));
    gtl_tracker = m_thTrackers.back().get();
    SetUpThreadFinishPromise(gtl_tracker);
}

void MemHawk::SetUpThreadFinishPromise(ThreadTracker* tracker)
{
    std::promise<uint32_t> exitPromise;
    exitPromise.set_value_at_thread_exit(tracker->trackerId);
    m_finishPromises.push_back(exitPromise.get_future());
}

void MemHawk::TrackAlloc(AllocInfo& info, Stacktrace&& trace)
{
    RecursionGuard<RetPtrTag> retPtrGuard;
    const auto span = trace.GetTrace();
    const auto level = retPtrGuard.Level();
    if (likely(span.size() > 1)) {
        gtl_retPtrs[level] = span[1];
    }
    auto retCleanup = absl::MakeCleanup([level]() { gtl_retPtrs[level] = nullptr; });

    RecursionGuard<AllocTag> guard;
    if (guard) {
        // external allocation
        if (unlikely(gtl_tracker == nullptr)) {
            RegisterThread();
        }
        gtl_tracker->SaveTraceId(info, std::move(trace));
        gtl_tracker->TrackAlloc(info);
    } else {
        // internal allocation of memhawk
        RecursionGuard<InnerAllocTag> innerGuard;
        // track only inner memhawk's stacktraces in order to reduce index size
        trace.ShrinkByPtr(gtl_retPtrs[level - 1]); // level can't be less than 1
        if (level > 1 && trace.GetTrace().size() >= 1) {
            // don't interested in previous memhawk call
            // malloc->trace->malloc and free->trace->malloc will be squashed to trace->malloc
            trace.ShrinkBySize(trace.GetTrace().size() - 1);
        }
        // Don't interested in fine-grained stacktraces
        if (trace.GetTrace().size() > 6) {
            trace.ShrinkBySize(6);
        }
        // Use coarse representation of stacktrace in order to reduce amount of stacktraces
        trace.CoarseToFunctionsStart();

        info.traceId = m_innerBtTracker.InsertStacktrace(std::move(trace));

        if (innerGuard) {
            ProcessPostponed();
            m_innerTracker->TrackAlloc(info);
        } else {
            PostponeAlloc(info);
        }
    }
}

void MemHawk::TrackDealloc(AllocInfo& info, const Stacktrace& trace)
{
    RecursionGuard<RetPtrTag> retPtrGuard;
    const auto span = trace.GetTrace();
    const auto level = retPtrGuard.Level();
    if (likely(span.size() > 1)) {
        gtl_retPtrs[level] = span[1];
    }
    auto retCleanup = absl::MakeCleanup([level]() { gtl_retPtrs[level] = nullptr; });

    RecursionGuard<AllocTag> guard;
    if (guard) {
        // external deallocation
        if (unlikely(gtl_tracker == nullptr)) {
            RegisterThread();
        }
        gtl_tracker->TrackDealloc(info);
    } else {
        // internal deallocation of memhawk
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
        LogDebug("processing: [size: " fU32 ", op: " fI32 "]", delayed.info.size, static_cast<int>(delayed.op));
        if (delayed.op == Postponed::Operation::Alloc) {
            m_innerTracker->TrackAlloc(delayed.info);
        } else {
            m_innerTracker->TrackDealloc(delayed.info);
        }
    } while (true);
}

void MemHawk::PostponeAlloc(const AllocInfo& info)
{
    if (m_postponed.size() >= m_postponedCapacity) {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .op = Postponed::Operation::Alloc};
    m_postponed.push_back(std::move(op));
    m_maxPostponedSize = std::max(m_maxPostponedSize, m_postponed.size());
}

void MemHawk::PostponeDealloc(const AllocInfo& info)
{
    if (m_postponed.size() >= m_postponedCapacity) {
        LogWarning("Skipped postponed alloc due to exhausting capacity");
        return;
    }
    Postponed op{.info = info, .op = Postponed::Operation::Free};
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

    m_workerData->summaryFile = std::ofstream(GetProcessLogName("summary", gl_config), std::ios_base::out | std::ios_base::trunc);
    m_workerData->stacktracesFile =
        std::ofstream(GetProcessLogName("stacktraces", gl_config), std::ios_base::out | std::ios_base::trunc);

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
        if (tracker->trackerId == gtl_tracker->trackerId) {
            // add tag in order not to deadlock
            RecursionGuard<AllocTag> guard;
            WorkerAccountThreadTracker(*tracker);
        } else {
            WorkerAccountThreadTracker(*tracker);
        }
    }
    {
        // add tags in order not to deadlock
        RecursionGuard<AllocTag> guard;
        RecursionGuard<InnerAllocTag> innerGuard;
        WorkerAccountThreadTracker(*m_innerTracker);
    }
}

void MemHawk::WorkerAccountThreadTracker(ThreadTracker& tracker)
{
    auto& byTraceIdIndex = m_workerData->index.get<WorkerData::ByTraceId>();
    absl::MutexLock trackerLock(&tracker.mt);
    for (const auto& [traceId, summary] : tracker.allocSummaries) {
        auto statIt = byTraceIdIndex.find(traceId);
        if (statIt == byTraceIdIndex.end()) {
            statIt = byTraceIdIndex.insert(WorkerData::IndexValue{traceId}).first;
        }
        const auto& localSummary = summary; // can be directly captured in lambda since c++20 only
        byTraceIdIndex.modify(statIt,
                              [&localSummary](WorkerData::IndexValue& value) { value.summary += localSummary; });
    }
    // ConsumeDiff should be the last call and be called once
    m_workerData->summary += tracker.total.ConsumeDiff();
    m_workerData->updatedTraces += tracker.allocSummaries.size();
    tracker.Clear();
}

void MemHawk::WorkerPrintData()
{
    absl::flat_hash_set<uint32_t> newStacktraces;

    const auto& bySizeIndex = m_workerData->index.get<WorkerData::ByTotalSize>();
    size_t topElementsCount = std::min(10ul, bySizeIndex.size());
    const auto bySizeRange = boost::make_iterator_range_n(bySizeIndex.begin(), topElementsCount);

    const auto& byCountIndex = m_workerData->index.get<WorkerData::ByTotalCount>();
    topElementsCount = std::min(10ul, byCountIndex.size());
    const auto byCountRange = boost::make_iterator_range_n(byCountIndex.begin(), topElementsCount);

    std::stringstream str;
    str << absl::FormatTime(absl::Now()) << "\n";
    str << fmt::format("Application heap: {:.3f}mb, active: {}, total: {}, memhawk overhead: {:.3f}mb\n",
                       m_workerData->summary.size / 1024.0 / 1024, m_workerData->summary.active,
                       m_workerData->summary.total, m_workerData->summary.overhead / 1024.0 / 1024);
    str << fmt::format("Total traces: {}, updated since last time: {}\n", m_workerData->index.size(),
                       m_workerData->updatedTraces);

    for (const auto& value : bySizeRange) {
        if (value.summary.active == 0) {
            continue;
        }
        const auto it = m_workerData->writtenStacktraces.insert(value.traceId);
        if (it.second) {
            newStacktraces.insert(value.traceId);
        }
        const auto average = value.summary.active == 0 ? 0.0 : 1.0 * value.summary.size / value.summary.active;
        str << fmt::format("TraceId: {}, active: {}, size: {:.3f}mb, average: {:.3f}b, total: {}\n", value.traceId,
                           value.summary.active, value.summary.size / 1024.0 / 1024, average, value.summary.total);
    }
    str << "\n";
    for (const auto& value : byCountRange) {
        if (value.summary.active == 0) {
            continue;
        }
        const auto it = m_workerData->writtenStacktraces.insert(value.traceId);
        if (it.second) {
            newStacktraces.insert(value.traceId);
        }
        const auto average = value.summary.active == 0 ? 0.0 : 1.0 * value.summary.size / value.summary.active;
        str << fmt::format("TraceId: {}, active: {}, size: {:.3f}mb, average: {:.3f}b, total: {}\n", value.traceId,
                           value.summary.active, value.summary.size / 1024.0 / 1024, average, value.summary.total);
    }
    str << "\n\n";
    m_workerData->summaryFile << str.str();

    for (const auto& traceId : newStacktraces) {
        auto trace = m_btTracker.GetStacktraceFromId(traceId);
        if (unlikely(!trace.has_value())) {
            trace = m_innerBtTracker.GetStacktraceFromId(traceId);
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

} // namespace memhawk
