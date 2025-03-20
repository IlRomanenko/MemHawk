#pragma once


#include <absl/base/attributes.h>
#include <absl/types/span.h>

#include <cstdint>
#include <limits>

namespace memhawk::bit_packing
{

template <typename ValueType>
struct BitIterator
{
    ValueType* value{};
    uint32_t bitIndex{};
    uint32_t valuePos{};

    constexpr BitIterator() = default;

    explicit BitIterator(const absl::Span<ValueType>& data) : value(data.data())
    {
    }

    explicit BitIterator(ValueType* data) : value(data)
    {
    }

    void AdvanceBytes(uint32_t cnt)
    {
        value += cnt;
        valuePos += cnt;
    }
};

using MutBitIterator = BitIterator<uint32_t>;
using ConstBitIterator = BitIterator<const uint32_t>;

constexpr const uint32_t ValueBitsSize = std::numeric_limits<uint32_t>::digits;
constexpr const uint32_t ValueBitsMask = ~static_cast<uint32_t>(0);

void EncodeBits(MutBitIterator& iter, uint32_t bitCount, uint32_t value);
void DecodeBits(ConstBitIterator& iter, uint32_t bitCount, uint32_t& value);

// Simple bit-packing algorithm
size_t CalculateMaxExpectedSize(absl::Span<const uint64_t> in);

size_t Compress(absl::Span<const uint64_t> in, uint32_t* out);
void Compress(absl::Span<const uint64_t> in, std::vector<uint32_t>& out);
size_t Decompress(absl::Span<const uint32_t> in, uint64_t* out);
void Decompress(absl::Span<const uint32_t> in, std::vector<uint64_t>& out);

} // namespace memhawk::bit_packing
