#include "text_writer.h"

#include "log_name.h"
#include "stacktrace.h"

#include <boost/range/iterator_range.hpp>

namespace memhawk
{

TextWriter::TextWriter(Config cfg, std::unique_ptr<IStacktraceFinder> finder)
    : m_cfg{std::move(cfg)}, m_stacktraceFinder{std::move(finder)}
{
}

void TextWriter::PostponedConstruct()
{
    m_storage = std::make_unique<Storage>();
    m_storage->summaryFile =
        std::ofstream(GetProcessLogName("summary", m_cfg), std::ios_base::out | std::ios_base::trunc);
    m_storage->stacktracesFile =
        std::ofstream(GetProcessLogName("stacktraces", m_cfg), std::ios_base::out | std::ios_base::trunc);
}

TextWriter::~TextWriter()
{
    m_storage->summaryFile.close();
    m_storage->stacktracesFile.close();
    LogInfo("Storage.index");
    for (const auto& elem : m_storage->index)
    {
        if (elem.summary.active == 0)
        {
            continue;
        }
        // if (!IsFixedTrackerId(elem.traceId))
        // {
        //     continue;
        // }
        LogInfo("TraceId: " fU32 ", active: " fI64 ", size: " fI64 ", overhead: " fI64 ", total: " fI64, elem.traceId,
                elem.summary.active, elem.summary.size, elem.summary.overhead, elem.summary.totalCount);
    }
}

void TextWriter::AccountThreadTracker(ThreadTracker* tracker)
{
    {
        auto lockedTracker = tracker->LockTracker();
        lockedTracker.ConsumeDiff(m_storage->localSummaries, m_storage->summary);
    }
    m_storage->updatedTraces += m_storage->localSummaries.size();

    auto& byTraceIdIndex = m_storage->index.get<Storage::ByTraceId>();
    for (const auto& [traceId, summary] : m_storage->localSummaries)
    {
        auto statIt = byTraceIdIndex.find(traceId);
        if (statIt == byTraceIdIndex.end())
        {
            statIt = byTraceIdIndex.insert(Storage::IndexValue{traceId}).first;
        }
        byTraceIdIndex.modify(statIt, [&summary](Storage::IndexValue& value) {
            value.changed = true;
            value.summary += summary;
        });
    }
    m_storage->localSummaries.clear();
}

void TextWriter::FlushData()
{
    absl::flat_hash_set<uint32_t> newStacktraces;

    const auto& bySizeIndex = m_storage->index.get<Storage::ByTotalSize>();
    size_t topElementsCount = std::min(m_cfg.TrackerBySizeCount, bySizeIndex.size());
    const auto bySizeRange = boost::make_iterator_range_n(bySizeIndex.begin(), topElementsCount);

    const auto& byCountIndex = m_storage->index.get<Storage::ByTotalCount>();
    topElementsCount = std::min(m_cfg.TrackerByTotalCount, byCountIndex.size());
    const auto byCountRange = boost::make_iterator_range_n(byCountIndex.begin(), topElementsCount);

    std::stringstream str;
    str << absl::FormatTime(absl::Now()) << "\n";
    str << fmt::format("Application heap: {:.3f}mb, active: {}, total: {}, memhawk overhead: {:.3f}mb\n",
                       static_cast<double>(m_storage->summary.size) / 1024 / 1024, m_storage->summary.active,
                       m_storage->summary.totalCount, static_cast<double>(m_storage->summary.overhead) / 1024 / 1024);
    str << fmt::format("Total traces: {}, updated since last time: {}\n", m_storage->index.size(),
                       m_storage->updatedTraces);

    str << "ByActiveSize" << "\n";
    for (const auto& value : bySizeRange)
    {
        if (value.summary.active == 0)
        {
            continue;
        }
        const auto it = m_storage->writtenStacktraces.insert(value.traceId);
        if (it.second)
        {
            newStacktraces.insert(value.traceId);
        }
        const auto average = value.summary.active == 0
                                 ? 0.0
                                 : static_cast<double>(value.summary.size) / static_cast<double>(value.summary.active);
        const auto totalMb = static_cast<double>(value.summary.size) / 1024.0 / 1024;
        str << fmt::format("TraceId: {}, active: {}, size: {:.3f}mb, average: {:.3f}b, total: {}\n", value.traceId,
                           value.summary.active, totalMb, average, value.summary.totalCount);
    }
    str << "\n";
    str << "ByTotalCount" << "\n";
    for (const auto& value : byCountRange)
    {
        const auto it = m_storage->writtenStacktraces.insert(value.traceId);
        if (it.second)
        {
            newStacktraces.insert(value.traceId);
        }
        const auto average = value.summary.active == 0
                                 ? 0.0
                                 : static_cast<double>(value.summary.size) / static_cast<double>(value.summary.active);
        const auto totalMb = static_cast<double>(value.summary.size) / 1024.0 / 1024;
        str << fmt::format("TraceId: {}, active: {}, size: {:.3f}mb, average: {:.3f}b, total: {}\n", value.traceId,
                           value.summary.active, totalMb, average, value.summary.totalCount);
    }
    str << "\n\n";
    m_storage->summaryFile << str.str();

    for (const auto& traceId : newStacktraces)
    {
        auto trace = m_stacktraceFinder->GetStacktraceFromId(traceId);

        if (!trace.has_value())
        {
            LogWarning("Missed stacktrace: " fU32, traceId);
            continue;
        }

        auto traceStr = trace.value().Describe();
        m_storage->stacktracesFile << fmt::format("TraceId: {}\n{}\n", traceId, traceStr) << std::endl;
    }
    m_storage->stacktracesFile.flush();
    m_storage->summaryFile.flush();
}

} // namespace memhawk
