#include "text_writer.h"

#include "logging.h"
#include "stacktrace.h"

#include <boost/range/iterator_range.hpp>
#include <fmt/format.h>

namespace memhawk
{

namespace writers
{

TextWriter::TextWriter(TextWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder)
    : m_cfg{std::move(cfg)}, m_stacktraceFinder{std::move(finder)}
{
    m_summaryFile = std::ofstream(GetProcessLogName("summary"), std::ios_base::out | std::ios_base::trunc);
    m_stacktracesFile = std::ofstream(GetProcessLogName("stacktraces"), std::ios_base::out | std::ios_base::trunc);
}

TextWriter::~TextWriter()
{
    m_summaryFile.close();
    m_stacktracesFile.close();
}

void TextWriter::UpdateModules()
{
}

void TextWriter::AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total)
{
    m_updatedTraces += summaries.size();
    m_summary += total;

    auto& byTraceIdIndex = m_index.get<ByTraceId>();
    for (const auto& [traceId, summary] : summaries)
    {
        auto statIt = byTraceIdIndex.find(traceId);
        if (statIt == byTraceIdIndex.end())
        {
            statIt = byTraceIdIndex.insert(IndexValue{traceId}).first;
        }
        byTraceIdIndex.modify(statIt, [&summary](IndexValue& value) {
            value.changed = true;
            value.summary += summary;
        });
    }
}

void TextWriter::FlushData()
{
    absl::flat_hash_set<uint32_t> newStacktraces;

    const auto& bySizeIndex = m_index.get<ByTotalSize>();
    size_t topElementsCount = std::min(*m_cfg.TrackerBySizeCount, bySizeIndex.size());
    const auto bySizeRange = boost::make_iterator_range_n(bySizeIndex.begin(), topElementsCount);

    const auto& byCountIndex = m_index.get<ByTotalCount>();
    topElementsCount = std::min(*m_cfg.TrackerByTotalCount, byCountIndex.size());
    const auto byCountRange = boost::make_iterator_range_n(byCountIndex.begin(), topElementsCount);

    std::stringstream str;
    str << absl::FormatTime(absl::Now()) << "\n";
    str << fmt::format("Application heap: {:.3f}mb, active: {}, total: {}, memhawk overhead: {:.3f}mb\n",
                       static_cast<double>(m_summary.size) / 1024 / 1024, m_summary.active, m_summary.totalCount,
                       static_cast<double>(m_summary.overhead) / 1024 / 1024);
    str << fmt::format("Total traces: {}, updated since last time: {}\n", m_index.size(), m_updatedTraces);

    str << "ByActiveSize" << "\n";
    for (const auto& value : bySizeRange)
    {
        if (value.summary.active == 0)
        {
            continue;
        }
        const auto it = m_writtenStacktraces.insert(value.traceId);
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
        const auto it = m_writtenStacktraces.insert(value.traceId);
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
    m_summaryFile << str.str();

    for (const auto& traceId : newStacktraces)
    {
        auto trace = m_stacktraceFinder->GetStacktraceFromId(traceId);

        if (!trace.has_value())
        {
            LogWarning("Missed stacktrace: " fU32, traceId);
            continue;
        }

        auto traceStr = trace.value().Describe();
        m_stacktracesFile << fmt::format("TraceId: {}\n{}\n", traceId, traceStr) << std::endl;
    }
    m_stacktracesFile.flush();
    m_summaryFile.flush();
}

} // namespace writers
} // namespace memhawk
