
#include "impl/compression.h"

#include <absl/types/span.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <random>

namespace memhawk::bit_packing
{

class CompressionFixture : public testing::Test
{
protected:
    std::vector<uint32_t> m_data;
    static constexpr const uint32_t TestValue = 0xDEADBEEF;
};

TEST_F(CompressionFixture, BitValueEncoded_ExpectOk)
{
    constexpr size_t Expected = 3;
    m_data.resize(1, TestValue);

    MutBitIterator iter{absl::MakeSpan(m_data)};
    EncodeBits(iter, 2, Expected);
    uint32_t value{};
    ConstBitIterator constIter{m_data};
    DecodeBits(constIter, 2, value);

    EXPECT_EQ(value, Expected);
}

TEST_F(CompressionFixture, BitEncode_Full32Bits_ExpectOk)
{
    constexpr size_t Expected = 3;
    m_data.resize(1, TestValue);

    MutBitIterator iter{absl::MakeSpan(m_data)};
    EncodeBits(iter, 32, Expected);
    uint32_t value{};
    ConstBitIterator constIter{m_data};
    DecodeBits(constIter, 32, value);

    EXPECT_EQ(value, Expected);
}

TEST_F(CompressionFixture, BitEncode_BetweenTwoBytes_ExpectOk)
{
    constexpr size_t Size = 2;
    m_data.resize(Size, TestValue);
    MutBitIterator iter{absl::MakeSpan(m_data)};

    const size_t BitCount = 24;
    for (size_t i = 0; i < Size; i++)
    {
        EncodeBits(iter, BitCount, i);
    }

    ConstBitIterator constIter{m_data};

    for (size_t i = 0; i < Size; i++)
    {
        uint32_t value{};
        DecodeBits(constIter, BitCount, value);
        EXPECT_EQ(value, i);
    }
}

TEST_F(CompressionFixture, BitEncode_SeveralBytes_ExpectOk)
{
    constexpr size_t Size = 32;
    m_data.resize(Size, TestValue);

    for (size_t bitCount = 5; bitCount <= 32; bitCount++)
    {
        MutBitIterator mutIter{absl::MakeSpan(m_data)};
        for (size_t i = 0; i < Size; i++)
        {
            EncodeBits(mutIter, bitCount, i);
        }
        ConstBitIterator constIter{m_data};

        for (size_t i = 0; i < Size; i++)
        {
            uint32_t value{};
            DecodeBits(constIter, bitCount, value);
            EXPECT_EQ(value, i);
        }
    }
}

TEST_F(CompressionFixture, Compress_SeveralBytes_ExpectOk)
{
    const std::vector<uint64_t> TestData = {TestValue, 0, 1, (1 << 16) + 8};

    Compress(absl::MakeConstSpan(TestData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(TestData, result);
}

TEST_F(CompressionFixture, Compress_Zeros_ExpectOk)
{
    constexpr const size_t Size = 32;
    std::vector<uint64_t> testData;
    testData.resize(Size, 0);

    Compress(absl::MakeConstSpan(testData), m_data);

    constexpr const size_t ExpectedSize = ((Size * (2 + 2)) + 31) / 32 + 1;
    EXPECT_EQ(m_data.size(), ExpectedSize);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_BoundaryValues_ExpectOk)
{
    std::vector<uint64_t> testData;
    testData.emplace_back(0);
    for (const auto bitCount : {8, 16, 24, 32, 40, 48, 56, 63})
    {
        const uint64_t value = 1ULL << bitCount;
        testData.emplace_back(value - 1);
        testData.emplace_back(value);
        testData.emplace_back(value + 1);
    }
    Compress(absl::MakeConstSpan(testData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_LargeDiffs_ExpectOk)
{
    const std::vector<uint64_t> TestData = {
        1ULL << 60, 1ULL << 12, 1ULL << 59, 0, 1ULL << 58, 1, 1ULL << 57,
    };

    Compress(absl::MakeConstSpan(TestData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(TestData, result);
}

TEST_F(CompressionFixture, Compress_SmallDiffs_ExpectOk)
{
    const std::vector<int64_t> TestDiffs = {1, 1LL << 16, 10, (1LL << 16) + 5, -10, 1LL << 25};
    std::vector<uint64_t> testData;
    testData.emplace_back(TestValue);
    for (size_t cnt = 0; cnt < 2; cnt++)
    {
        for (const auto& elem : TestDiffs)
        {
            const auto value = static_cast<int64_t>(testData.back()) + elem;
            testData.emplace_back(static_cast<uint64_t>(value));
        }
    }

    Compress(absl::MakeConstSpan(testData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);


    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_IterativeDiffs_ExpectOk)
{
    constexpr size_t MaxDiff = 1UL << 25;
    std::vector<uint64_t> testData;
    testData.emplace_back(TestValue);
    for (size_t cnt = 0; cnt < 2; cnt++)
    {
        for (size_t i = 0; i < MaxDiff; i++)
        {
            testData.emplace_back(testData.back() + i);
        }
        for (size_t i = 0; i < MaxDiff; i++)
        {
            testData.emplace_back(testData.back() - i);
        }
    }

    Compress(absl::MakeConstSpan(testData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_SignIterativeDiffs_ExpectOk)
{
    constexpr size_t MaxDiff = 1UL << 25;
    std::vector<uint64_t> testData;
    testData.emplace_back(TestValue);
    for (size_t cnt = 0; cnt < 2; cnt++)
    {
        for (size_t i = 0; i < MaxDiff; i++)
        {
            if ((i + cnt) & 1)
            {
                testData.emplace_back(testData.back() + i);
            }
            else
            {
                testData.emplace_back(testData.back() - i);
            }
        }
    }

    Compress(absl::MakeConstSpan(testData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_SomePtrs_ExpectOk)
{
    const std::vector<uint64_t> TestData = {0x7514af7f7063, 0x7514af839744, 0x7514af8049eb,
                                            0x7514af80d866, 0x7514af80b092, 0x7514af80b5b4};

    Compress(absl::MakeConstSpan(TestData), m_data);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(TestData, result);
}

TEST_F(CompressionFixture, Compress_RandomData_ExpectOk)
{
    std::mt19937_64 rng(42); // NOLINT(bugprone-random-generator-seed)
    std::uniform_int_distribution<> dist;

    constexpr const size_t Size = 1'000'000;
    std::vector<uint64_t> testData;
    testData.reserve(Size);

    for (size_t i = 0; i < Size; i++)
    {
        testData.emplace_back(dist(rng));
    }

    Compress(absl::MakeConstSpan(testData), m_data);

    EXPECT_LT(m_data.size() * sizeof(uint32_t), testData.size() * sizeof(uint64_t));

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_SmallData_ExpectBufferCleared)
{
    std::vector<uint32_t> clearValues;
    Compress(absl::MakeConstSpan({128UL}), clearValues);
    EXPECT_EQ(clearValues.size(), 3);

    std::vector<uint32_t> dirtyValues(3, std::numeric_limits<uint32_t>::max());
    Compress(absl::MakeConstSpan({128UL}), dirtyValues);
    EXPECT_EQ(dirtyValues.size(), 3);

    EXPECT_EQ(clearValues, dirtyValues);
}
} // namespace memhawk::bit_packing
