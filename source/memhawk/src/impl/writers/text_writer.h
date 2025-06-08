#pragma once

#include "alloc_info.h"
#include "config.h"
#include "i_writer.h"

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

#include <fstream>

namespace memhawk
{
namespace writers
{

namespace bmi = boost::multi_index;

class TextWriter : public IWriterStrategy
{
public:
    explicit TextWriter(TextWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder);
    ~TextWriter() override;

    void UpdateModules() override;
    void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) override;
    void FlushData() override;

private:
    struct IndexValue
    {
        uint32_t traceId{};
        mutable AllocSummary summary;

        constexpr explicit IndexValue(uint32_t id) : traceId{id}
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
    using Index = bmi::multi_index_container<
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
    >;
    // clang-format on

    void AccountSummary(Index& index, AllocSummary& total, uint32_t traceId, const AllocSummary& summary);

    void DumpIndex(const Index& index, const TextWriterIndexConfig& cfg, std::stringstream& str,
                   absl::flat_hash_set<uint32_t>& newStacktraces);

private:
    TextWriterConfig m_cfg;
    std::shared_ptr<IStacktraceFinder> m_stacktraceFinder;

    std::ofstream m_summaryFile;
    std::ofstream m_stacktracesFile;

    absl::flat_hash_set<uint32_t> m_writtenStacktraces;
    size_t m_updatedTraces{};

    AllocSummary m_total;

    Index m_externalAllocsIndex;
    AllocSummary m_externalAllocsSummary;

    Index m_internalAllocsIndex;
    AllocSummary m_internalAllocsSummary;
};

} // namespace writers
} // namespace memhawk
