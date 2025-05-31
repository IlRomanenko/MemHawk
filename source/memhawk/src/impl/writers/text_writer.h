#pragma once

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
namespace bmi = boost::multi_index;

class TextWriter : public IWriter
{
public:
    explicit TextWriter(Config cfg, std::unique_ptr<IStacktraceFinder> finder);
    ~TextWriter() override;

    void PostponedConstruct() override;
    void AccountThreadTracker(ThreadTracker* tracker) override;
    void FlushData() override;

private:
    struct Storage
    {
        std::ofstream summaryFile;
        std::ofstream stacktracesFile;
        absl::flat_hash_set<uint32_t> writtenStacktraces;
        AllocSummary summary;
        size_t updatedTraces{};

        absl::flat_hash_map<uint32_t, AllocSummary> localSummaries;

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
        bmi::multi_index_container<
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
        > index;
        // clang-format on
    };

private:
    Config m_cfg;
    std::unique_ptr<IStacktraceFinder> m_stacktraceFinder;
    std::unique_ptr<Storage> m_storage;
};

} // namespace memhawk
