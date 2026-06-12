#include "text_writer.h"

#include "alloc_info.h"
#include "config.h"
#include "logging.h"
#include "stacktrace.h"
#include "trackers/stacktrace_tracker_static.h"

#include <boost/range/iterator_range.hpp>
#include <fmt/format.h>

#include <cstdint>
#include <ostream>
#include <sstream>

namespace memhawk
{

namespace writers
{

namespace
{
std::string ToString(const AllocSummary& summary)
{
    const auto totalMb = static_cast<double>(summary.size) / 1024 / 1024;
    const auto overheadMb = static_cast<double>(summary.overhead) / 1024 / 1024;
    const auto averageBytes =
        summary.active == 0 ? 0.0 : static_cast<double>(summary.size) / static_cast<double>(summary.active);
    return fmt::format(
        "size: {:>9.3f} mb, active: {:>12}, total: {:>12}, average: {:>12.3f} bytes, overhead: {:>9.3f} mb", totalMb,
        summary.active, summary.totalCount, averageBytes, overheadMb);
}

} // namespace

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

void TextWriter::AccountSummary(Index& index, AllocSummary& total, uint32_t traceId, const AllocSummary& summary)
{
    total += summary;
    auto& byTraceIdIndex = index.template get<ByTraceId>();
    auto statIt = byTraceIdIndex.find(traceId);
    if (statIt == byTraceIdIndex.end())
    {
        statIt = byTraceIdIndex.insert(IndexValue{traceId}).first;
    }
    byTraceIdIndex.modify(statIt, [&summary](IndexValue& value) { value.summary += summary; });
}

void TextWriter::AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total)
{
    m_updatedTraces += summaries.size();
    m_total += total;

    for (const auto& [traceId, summary] : summaries)
    {
        const auto isinternal = IsFixedTrackerId(traceId);
        auto& index = isinternal ? m_internalAllocsIndex : m_externalAllocsIndex;
        auto& summaryTotal = isinternal ? m_internalAllocsSummary : m_externalAllocsSummary;
        AccountSummary(index, summaryTotal, traceId, summary);
    }
}

void TextWriter::DumpIndex(const Index& index, const TextWriterIndexConfig& cfg, std::stringstream& str,
                           absl::flat_hash_set<uint32_t>& newStacktraces)
{
    if (!*cfg.Enabled)
    {
        return;
    }
    const auto& bySizeIndex = index.get<ByTotalSize>();
    size_t topElementsCount = std::min(*cfg.TrackerBySizeCount, bySizeIndex.size());
    const auto bySizeRange = boost::make_iterator_range_n(bySizeIndex.begin(), topElementsCount);
    
    const auto& byCountIndex = index.get<ByTotalCount>();
    topElementsCount = std::min(*cfg.TrackerByTotalCount, byCountIndex.size());
    const auto byCountRange = boost::make_iterator_range_n(byCountIndex.begin(), topElementsCount);

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
        str << fmt::format("TraceId: {:>9}, {}\n", value.traceId, ToString(value.summary));
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
        str << fmt::format("TraceId: {:>9}, {}\n", value.traceId, ToString(value.summary));
    }
}

void TextWriter::FlushData()
{
    absl::flat_hash_set<uint32_t> newStacktraces;

    std::stringstream str;
    str << absl::FormatTime(absl::Now()) << "\n";
    str << fmt::format("Application heap: {}\n", ToString(m_externalAllocsSummary));
    str << fmt::format("MemHawk heap:     {}\n", ToString(m_internalAllocsSummary));
    str << fmt::format("Total traces: {}, updated since last time: {}\n",
                       m_externalAllocsIndex.size() + m_internalAllocsIndex.size(), m_updatedTraces);

    str << "External index:" << "\n";
    DumpIndex(m_externalAllocsIndex, *m_cfg.ExternalTraces, str, newStacktraces);
    str << "\n\n";
    str << "Internal index:" << "\n";
    DumpIndex(m_internalAllocsIndex, *m_cfg.InternalTraces, str, newStacktraces);
    str << "\n\n";

    m_summaryFile << str.str();

    for (const auto& traceId : newStacktraces)
    {
        auto trace = m_stacktraceFinder->GetStacktraceFromId(traceId);

        if (!trace.has_value())
        {
            LogWarning("Missing stacktrace: " fU32, traceId);
            continue;
        }

        auto traceStr = trace.value().Describe();
        m_stacktracesFile << fmt::format("TraceId: {}\n{}\n", traceId, traceStr) << std::endl;
    }
    m_stacktracesFile.flush();
    m_summaryFile.flush();
    m_updatedTraces = 0;
}

} // namespace writers
} // namespace memhawk
