#include "gtest/gtest.h"
#include "impl/algo.h"

#include <absl/types/span.h>
#include <gtest/gtest-param-test.h>

#include <cstdint>

namespace memhawk
{

class CollapseRecursionFixture : public testing::TestWithParam<size_t (*)(absl::Span<void*>, size_t)>
{
public:
    template <typename T>
    void SetUpData(std::vector<T> data)
    {
        for (const auto& elem : data)
        {
            m_data.push_back(static_cast<void*>(elem));
        }
    }

    void SetUpData(const std::string& data)
    {
        for (const auto& elem : data)
        {
            m_data.push_back(reinterpret_cast<void*>(static_cast<uint64_t>(elem)));
        }
    }

    std::string DecodeData(size_t size)
    {
        std::string result;
        for (size_t i = 0; i < size; i++)
        {
            result += static_cast<char>(reinterpret_cast<uint64_t>(m_data[i]));
        }
        return result;
    }

protected:
    std::vector<void*> m_data;
    static constexpr const uint32_t TestValue = 0xDEADBEEF;
};

INSTANTIATE_TEST_SUITE_P(CollapseRecursion, CollapseRecursionFixture,
                         testing::Values(CollapseRecursionNaive, CollapseRecursion, CollapseRecursionOpt));

TEST_P(CollapseRecursionFixture, Collapse_Depth0_ExpectNotCollapsed)
{
    constexpr const char* TestData = "ccdabacabababacafaaaf";
    constexpr const char* Expected = TestData;
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 0);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_AllEqualDepth1_ThreeChars_ExpectOneLeft)
{
    constexpr const char* TestData = "aaa";
    constexpr const char* Expected = "a";
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 1);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_AllEqualDepth1_ExpectOneLeft)
{
    constexpr const char* TestData = "aaaaaaa";
    constexpr const char* Expected = "a";
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 1);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_OneGroup_Size_2_Depth_2_ExpectOneLeft)
{
    constexpr const char* TestData = "ababababababababab";
    constexpr const char* Expected = "ab";
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 2);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_OneGroup_Size_2_Depth_Max_ExpectOneLeft)
{
    constexpr const char* TestData = "ababababababababab";
    constexpr const char* Expected = "ab";
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), MaxCollapseDepth);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_OneGroup_Size_4_Depth_Max_ExpectOneLeft)
{
    constexpr const char* TestData = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd";
    constexpr const char* Expected = "abcd";
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), MaxCollapseDepth);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_Depth1_ExpectSingleCharCollapsed)
{
    constexpr const char* TestData = "ccdabacabababacafaaaf";
    constexpr const char* Expected = "cdabacabababacafaf"; // collapse 'c', 'a' groups
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 1);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_ExpectOk)
{
    constexpr const char* TestData = "ccdabacabababacafaaaf";
    constexpr const char* Expected = "cdabacabacafaf"; // collapse 'c', 'ab', 'a' groups
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 4);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_MaxDepth_ExpectOk)
{
    constexpr const char* TestData = "ccdabacabababacafaaaf";
    constexpr const char* Expected = "cdabacabacafaf"; // collapse 'c', 'a' groups
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), MaxCollapseDepth);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_BracedAllEqual_ExpectOneLeftInside)
{
    constexpr const char* TestData = "faaaaaaaaaaaaaaaaaaaaaaaaaad";
    constexpr const char* Expected = "fad"; // collapse 'a' groups
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), MaxCollapseDepth);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

TEST_P(CollapseRecursionFixture, Collapse_ZeroGroups_ExpectLeftSame)
{
    constexpr const char* TestData = "abacaba";
    constexpr const char* Expected = TestData; // nothing to compress
    SetUpData(TestData);
    const size_t newSize = GetParam()(absl::MakeSpan(m_data), 4);
    const auto result = DecodeData(newSize);
    EXPECT_EQ(result, Expected);
}

} // namespace memhawk
