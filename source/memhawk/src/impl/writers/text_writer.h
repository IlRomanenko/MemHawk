#pragma once

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
        bool changed{};
        uint32_t traceId{};
        mutable AllocSummary summary;

        explicit IndexValue(uint32_t id) : changed{true}, traceId{id}
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
    struct ByChangedFlag{};
    using Index = bmi::multi_index_container<
        IndexValue,
        bmi::indexed_by<
            bmi::ordered_non_unique<
                bmi::tag<ByChangedFlag>,
                bmi::member<IndexValue, bool, &IndexValue::changed>,
                std::greater<>
            >,
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
private:
    TextWriterConfig m_cfg;
    std::shared_ptr<IStacktraceFinder> m_stacktraceFinder;

    std::ofstream m_summaryFile;
    std::ofstream m_stacktracesFile;

    absl::flat_hash_set<uint32_t> m_writtenStacktraces;
    AllocSummary m_summary;
    size_t m_updatedTraces{};

    Index m_index;
};

} // namespace writers
} // namespace memhawk
