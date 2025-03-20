
#include "impl/stacktrace.h"
#include "impl/stacktrace_tracker_static.h"

#include <absl/types/span.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <random>

namespace memhawk::bit_packing
{

// todo: same flag is defined inside stacktrace_tracker_fixed_size
constexpr uint32_t FixedTrackerIdFlag = 1UL << (std::numeric_limits<uint32_t>::digits - 1);

class StaticStacktraceTrackerFixture : public testing::Test
{
public:
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

    static uint32_t GetFixedSizeTraceId(uint32_t traceId)
    {
        return traceId ^ FixedTrackerIdFlag;
    }

protected:
    std::vector<uint32_t> m_data;
    StaticStacktraceTracker m_tracker;
    static constexpr const uint32_t TestValue = 0xDEADBEEF;
};

TEST_F(StaticStacktraceTrackerFixture, AddTrace_ExpectFound)
{
    const std::vector<uint64_t> testData = {0x7514af7f7063, 0x7514af839744, 0x7514af8049eb,
                                            0x7514af80d866, 0x7514af80b092, 0x7514af80b5b4};
    auto trace = SetUpStacktrace(testData);
    const size_t traceId = m_tracker.InsertStacktrace(std::move(trace));
    EXPECT_EQ(traceId, GetFixedSizeTraceId(0));
    const auto foundTrace = m_tracker.GetStacktraceFromId(traceId);
    EXPECT_TRUE(foundTrace);
    EXPECT_EQ(ConvertStacktrace(foundTrace.value()), testData);
}

TEST_F(StaticStacktraceTrackerFixture, AddRandomTraces_ExpectAllFound)
{
    constexpr size_t Count = 128;
    std::vector<std::vector<uint64_t>> testData;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<> dist;


    for (size_t i = 0; i < Count; i++) {
        std::vector<uint64_t> trace;
        trace.reserve(StaticStacktraceTracker::MaxStacktraceLength);
        for (size_t j = 0; j < StaticStacktraceTracker::MaxStacktraceLength; j++) {
            trace.emplace_back(dist(rng));
        }
        testData.emplace_back(trace); // copy trace
        const auto traceId = m_tracker.InsertStacktrace(SetUpStacktrace(trace));
        EXPECT_EQ(traceId, GetFixedSizeTraceId(i));
    }

    for (size_t i = 0; i < Count; i++) {
        const auto foundTrace = m_tracker.GetStacktraceFromId(GetFixedSizeTraceId(i));
        EXPECT_TRUE(foundTrace);
        EXPECT_EQ(ConvertStacktrace(foundTrace.value()), testData[i]);
    }
}

} // namespace memhawk::bit_packing
