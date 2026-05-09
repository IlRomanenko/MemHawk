#include "compression.h"

#include <cstdint>

namespace memhawk::bit_packing
{

enum class SizeBits : uint32_t
{
    Zero = 0,
    TwoBytes = 1,
    ThreeBytes = 2,
    FourBytes = 3
};

inline uint32_t shift_left(uint32_t value, uint32_t count)
{
    if (count >= ValueBitsSize)
    {
        return 0;
    }
    return value << count;
}

inline uint32_t shift_right(uint32_t value, uint32_t count)
{
    if (count >= ValueBitsSize)
    {
        return 0;
    }
    return value >> count;
}

inline void ClearBits(uint32_t& value, uint32_t from, uint32_t to)
{
    const uint32_t upper = value & shift_left(ValueBitsMask, from);
    const uint32_t lower = upper & shift_right(ValueBitsMask, (ValueBitsSize - to));
    value &= ~lower;
}

void ClearUnusedBits(MutBitIterator& iter)
{
    if (iter.bitIndex == 0)
    {
        return;
    }
    ClearBits(*iter.value, iter.bitIndex, ValueBitsSize);
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE
void EncodeBits(MutBitIterator& iter, uint32_t bitCount, uint32_t value)
{
    while (bitCount > 0)
    {
        const uint32_t maxBitDiff = ValueBitsSize - iter.bitIndex;
        const uint32_t curBitsCount = maxBitDiff >= bitCount ? bitCount : maxBitDiff;

        const uint32_t BitsMask = ~shift_left(ValueBitsMask, curBitsCount);
        const uint32_t curBitsValue = value & BitsMask;
        value = shift_right(value, curBitsCount);
        ClearBits(*iter.value, iter.bitIndex, iter.bitIndex + curBitsCount);
        *iter.value |= shift_left(curBitsValue, iter.bitIndex);

        bitCount -= curBitsCount;
        iter.bitIndex += curBitsCount;
        if (iter.bitIndex >= ValueBitsSize)
        {
            iter.bitIndex = 0;
            iter.value++;
            iter.valuePos++;
        }
    }
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE
void DecodeBits(ConstBitIterator& iter, uint32_t bitCount, uint32_t& value)
{
    value = 0;
    uint32_t decodedBits = 0;
    while (bitCount > 0)
    {
        const uint32_t maxBitDiff = ValueBitsSize - iter.bitIndex;
        const uint32_t curBitsCount = maxBitDiff >= bitCount ? bitCount : maxBitDiff;

        const uint32_t BitsMask = ~shift_left(ValueBitsMask, curBitsCount);
        const uint32_t curBitsValue = shift_right(*iter.value, iter.bitIndex) & BitsMask;
        value |= shift_left(curBitsValue, decodedBits);

        decodedBits += curBitsCount;
        bitCount -= curBitsCount;
        iter.bitIndex += curBitsCount;
        if (iter.bitIndex >= ValueBitsSize)
        {
            iter.bitIndex = 0;
            iter.value++;
            iter.valuePos++;
        }
    }
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE
void Encode(MutBitIterator& controlIter, MutBitIterator& dataIter, uint32_t value)
{
    uint32_t bitCount = 0;
    if (value == 0)
    {
        EncodeBits(controlIter, 2, static_cast<uint32_t>(SizeBits::Zero));
    }
    else if (value < (1UL << 16))
    {
        EncodeBits(controlIter, 2, static_cast<uint32_t>(SizeBits::TwoBytes));
        bitCount = 16;
    }
    else if (value < (1UL << 24))
    {
        EncodeBits(controlIter, 2, static_cast<uint32_t>(SizeBits::ThreeBytes));
        bitCount = 24;
    }
    else
    {
        EncodeBits(controlIter, 2, static_cast<uint32_t>(SizeBits::FourBytes));
        bitCount = 32;
    }
    if (bitCount)
    {
        EncodeBits(dataIter, bitCount, value);
    }
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE
void Decode(ConstBitIterator& controlIter, ConstBitIterator& dataIter, uint32_t& value)
{
    value = 0;
    uint32_t sizeBits = 0;
    DecodeBits(controlIter, 2, sizeBits);
    switch (static_cast<SizeBits>(sizeBits))
    {
    case SizeBits::Zero:
        break;
    case SizeBits::TwoBytes:
        DecodeBits(dataIter, 16, value);
        break;
    case SizeBits::ThreeBytes:
        DecodeBits(dataIter, 24, value);
        break;
    case SizeBits::FourBytes:
        DecodeBits(dataIter, 32, value);
        break;
    default:
        ABSL_UNREACHABLE();
    }
}

size_t CalculateControlBlockSize(size_t size)
{
    const size_t blockSizeBits = size * 2 * 2;
    return (blockSizeBits + ValueBitsSize - 1) / ValueBitsSize;
}

uint32_t CalculateMaxExpectedSize(absl::Span<const uint64_t> in)
{
    return 1 + in.size() * 2 + CalculateControlBlockSize(in.size());
}

uint32_t Compress(const absl::Span<const uint64_t> in, uint32_t* out)
{
    out[0] = static_cast<uint32_t>(in.size());
    MutBitIterator controlBlockIter{out};
    controlBlockIter.AdvanceBytes(1);
    MutBitIterator dataIter{out};
    dataIter.AdvanceBytes(1 + CalculateControlBlockSize(in.size()));

    uint64_t prev = 0;
    uint64_t cur = 0;
    uint64_t diff = 0;

    for (size_t i = 0; i < in.size(); i++)
    {
        cur = in[i];

        diff = cur - prev;

        const auto high = static_cast<uint32_t>(diff >> 32);
        const auto low = static_cast<uint32_t>(diff);

        Encode(controlBlockIter, dataIter, low);
        Encode(controlBlockIter, dataIter, high);

        prev = in[i];
    }
    ClearUnusedBits(dataIter);
    ClearUnusedBits(controlBlockIter);
    if (dataIter.bitIndex == 0)
    {
        return dataIter.valuePos;
    }
    return dataIter.valuePos + 1; // because last word is also used
}

void Compress(absl::Span<const uint64_t> in, std::vector<uint32_t>& out)
{
    out.clear();
    out.resize(CalculateMaxExpectedSize(in));
    auto size = Compress(in, out.data());
    out.resize(size);
}

void Decompress(absl::Span<const uint32_t> in, std::vector<uint64_t>& out)
{
    const uint32_t total = in[0];
    out.clear();
    out.resize(total);
    Decompress(in, out.data());
}

uint32_t Decompress(absl::Span<const uint32_t> in, uint64_t* out)
{
    const uint32_t total = in[0];
    ConstBitIterator controlBlockIter{in};
    controlBlockIter.AdvanceBytes(1);
    ConstBitIterator dataIter{in};
    dataIter.AdvanceBytes(1 + CalculateControlBlockSize(total));

    uint64_t prev = 0;
    uint64_t cur = 0;
    uint64_t diff = 0;

    uint32_t high = 0;
    uint32_t low = 0;

    for (uint32_t i = 0; i < total; i++)
    {
        Decode(controlBlockIter, dataIter, low);
        Decode(controlBlockIter, dataIter, high);

        diff = (static_cast<uint64_t>(high) << 32) | low;

        cur = prev + diff;

        out[i] = cur;
        prev = cur;
    }
    return total;
}

} // namespace memhawk::bit_packing
