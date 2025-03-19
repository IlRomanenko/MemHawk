
#include "impl/compression.h"

#include <absl/types/span.h>
#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>

#include <cstdint>
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

    BitIterator iter{m_data};
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

    BitIterator iter{m_data};
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
    BitIterator iter{m_data};

    size_t bitCount = 24;
    for (size_t i = 0; i < Size; i++) {
        EncodeBits(iter, bitCount, i);
    }

    ConstBitIterator constIter{m_data};

    for (size_t i = 0; i < Size; i++) {
        uint32_t value{};
        DecodeBits(constIter, bitCount, value);
        EXPECT_EQ(value, i);
    }
}

TEST_F(CompressionFixture, BitEncode_SeveralBytes_ExpectOk)
{
    constexpr size_t Size = 32;
    m_data.resize(Size, TestValue);

    for (size_t bitCount = 5; bitCount <= 32; bitCount++) {
        MutBitIterator mutIter{m_data};
        for (size_t i = 0; i < Size; i++) {
            EncodeBits(mutIter, bitCount, i);
        }
        ConstBitIterator constIter{m_data};

        for (size_t i = 0; i < Size; i++) {
            uint32_t value{};
            DecodeBits(constIter, bitCount, value);
            EXPECT_EQ(value, i);
        }
    }
}

TEST_F(CompressionFixture, Compress_SeveralBytes_ExpectOk)
{
    const std::vector<uint64_t> TestData = {TestValue, 0, 1, (1<<16) + 8};

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
    
    constexpr const size_t ExpectedSize = ((Size * (1 + 2 + 2)) + 31) / 32 + 1;
    EXPECT_EQ(ExpectedSize, 6);
    EXPECT_EQ(m_data.size(), ExpectedSize);

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

TEST_F(CompressionFixture, Compress_LargeDiffs_ExpectOk)
{
    std::vector<uint64_t> testData = {1ull << 60, 1ul << 12, 1ul << 59, 0, 1ul << 58, 1, 1ul << 57};

    Compress(absl::MakeConstSpan(testData), m_data);
    
    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}


TEST_F(CompressionFixture, Compress_RandomData_ExpectOk)
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<> dist;

    constexpr const size_t Size = 1'000'000;
    std::vector<uint64_t> testData;
    testData.reserve(Size);

    for (size_t i = 0; i < Size; i++) {
        testData.emplace_back(dist(rng));
    }

    Compress(absl::MakeConstSpan(testData), m_data);

    EXPECT_LT(m_data.size() * sizeof(uint32_t), testData.size() * sizeof(uint64_t));

    std::vector<uint64_t> result;
    Decompress(absl::MakeConstSpan(m_data), result);

    EXPECT_EQ(testData, result);
}

} // namespace memhawk::bit_packing
