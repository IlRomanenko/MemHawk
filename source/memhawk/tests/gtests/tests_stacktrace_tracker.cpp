
#include "impl/config.h"
#include "impl/stacktrace.h"
#include "impl/stacktrace_tracker.h"

#include <absl/container/flat_hash_set.h>
#include <absl/types/span.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <random>

namespace memhawk::bit_packing
{

class StacktraceTrackerFixture : public testing::Test
{
public:

    void SetUp() override
    {
        m_tracker.PostponedConstruct();
    }

    static Stacktrace SetUpStacktrace(const std::vector<uint64_t>& data)
    {
        return {reinterpret_cast<void* const*>(data.data()), data.size()};
    }

    static absl::Span<const uint64_t> ConvertStacktrace(const Stacktrace& stacktrace)
    {
        const auto trace = stacktrace.GetTrace();
        ;
        return absl::MakeConstSpan(reinterpret_cast<const uint64_t*>(trace.data()), trace.size());
    }

protected:
    std::vector<uint32_t> m_data;
    StacktraceTracker m_tracker;
    static constexpr const uint32_t TestValue = 0xDEADBEEF;
};

TEST_F(StacktraceTrackerFixture, AddTrace_ExpectFound)
{
    const std::vector<uint64_t> testData = {0x7514af7f7063, 0x7514af839744, 0x7514af8049eb,
                                            0x7514af80d866, 0x7514af80b092, 0x7514af80b5b4};
    auto trace = SetUpStacktrace(testData);
    const size_t traceId = m_tracker.InsertStacktrace(std::move(trace));
    EXPECT_NE(traceId, 0);
    const auto foundTrace = m_tracker.GetStacktraceFromId(traceId);
    ASSERT_TRUE(foundTrace);
    EXPECT_EQ(ConvertStacktrace(foundTrace.value()), testData); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_F(StacktraceTrackerFixture, AddRandomTraces_ExpectAllFound)
{
    constexpr size_t Count = 4096;
    std::vector<std::vector<uint64_t>> testData;
    std::vector<uint32_t> savedTraceId;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<> dist;


    for (size_t i = 0; i < Count; i++)
    {
        std::vector<uint64_t> trace;
        trace.reserve(MaxUnwindDepth);
        for (size_t j = 0; j < MaxUnwindDepth; j++)
        {
            trace.emplace_back(dist(rng));
        }
        testData.emplace_back(trace); // copy trace
        const auto traceId = m_tracker.InsertStacktrace(SetUpStacktrace(trace));
        EXPECT_NE(traceId, 0);
        savedTraceId.push_back(traceId);
    }

    const absl::flat_hash_set<uint32_t> savedSet(savedTraceId.begin(), savedTraceId.end());
    EXPECT_EQ(savedSet.size(), savedTraceId.size());

    for (size_t i = 0; i < Count; i++)
    {
        const auto foundTrace = m_tracker.GetStacktraceFromId(savedTraceId[i]);
        ASSERT_TRUE(foundTrace);
        EXPECT_EQ(ConvertStacktrace(foundTrace.value()), testData[i]); // NOLINT(bugprone-unchecked-optional-access)
    }
}

} // namespace memhawk::bit_packing
