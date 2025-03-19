#include "compression.h"

#include <cstdint>

namespace memhawk
{

namespace bit_packing
{

enum class SizeBits : uint32_t
{
    Zero = 0,
    TwoBytes = 1,
    ThreeBytes = 2,
    FourBytes = 3
};

enum class SignBit : uint32_t
{
    Unsigned = 0,
    Signed = 1
};

inline uint32_t shift_left(uint32_t value, uint32_t count)
{
    if (count >= ValueBitsSize) {
        return 0;
    }
    return value << count;
}

inline uint32_t shift_right(uint32_t value, uint32_t count)
{
    if (count >= ValueBitsSize) {
        return 0;
    }
    return value >> count;
}

inline void ClearBits(uint32_t& value, uint32_t from, uint32_t to)
{
    uint32_t upper = value & shift_left(ValueBitsMask, from);
    uint32_t lower = upper & shift_right(ValueBitsMask, (ValueBitsSize - to));
    value &= ~lower; 
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE 
void EncodeBits(MutBitIterator& iter, uint32_t bitCount, uint32_t value)
{
    while (bitCount > 0) {
        const uint32_t maxBitDiff = ValueBitsSize - iter.bitIndex;
        const uint32_t curBitsCount = maxBitDiff >= bitCount ? bitCount : maxBitDiff;

        const uint32_t BitsMask = ~shift_left(ValueBitsMask, curBitsCount);
        const uint32_t curBitsValue = value & BitsMask;
        value = shift_right(value, curBitsCount);
        ClearBits(*iter.value, iter.bitIndex, iter.bitIndex + curBitsCount);
        *iter.value |= shift_left(curBitsValue, iter.bitIndex);

        bitCount -= curBitsCount;
        iter.bitIndex += curBitsCount;
        if (iter.bitIndex >= ValueBitsSize) {
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
    while (bitCount > 0) {
        const uint32_t maxBitDiff = ValueBitsSize - iter.bitIndex;
        const uint32_t curBitsCount = maxBitDiff >= bitCount ? bitCount : maxBitDiff;

        const uint32_t BitsMask = ~shift_left(ValueBitsMask, curBitsCount);
        const uint32_t curBitsValue = shift_right(*iter.value, iter.bitIndex) & BitsMask;
        value |= shift_left(curBitsValue, decodedBits);

        decodedBits += curBitsCount;
        bitCount -= curBitsCount;
        iter.bitIndex += curBitsCount;
        if (iter.bitIndex >= ValueBitsSize) {
            iter.bitIndex = 0;
            iter.value++;
            iter.valuePos++;
        }
    }
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE 
void Encode(MutBitIterator& iter, uint32_t value)
{
    if (value == 0) {
        EncodeBits(iter, 2, static_cast<uint32_t>(SizeBits::Zero));
    } else if (value <= (1ul << 16)) {
        EncodeBits(iter, 2, static_cast<uint32_t>(SizeBits::TwoBytes));
        EncodeBits(iter, 16, value);
    } else if (value <= (1ul << 24)) {
        EncodeBits(iter, 2, static_cast<uint32_t>(SizeBits::ThreeBytes));
        EncodeBits(iter, 24, value);
    } else {
        EncodeBits(iter, 2, static_cast<uint32_t>(SizeBits::FourBytes));
        EncodeBits(iter, 32, value);
    }
}

// ABSL_ATTRIBUTE_ALWAYS_INLINE 
void Decode(ConstBitIterator& iter, uint32_t& value)
{
    value = 0;
    uint32_t sizeBits = 0;
    DecodeBits(iter, 2, sizeBits);
    switch (static_cast<SizeBits>(sizeBits)) {
    case SizeBits::Zero:
        break;
    case SizeBits::TwoBytes:
        DecodeBits(iter, 16, value);
        break;
    case SizeBits::ThreeBytes:
        DecodeBits(iter, 24, value);
        break;
    case SizeBits::FourBytes:
        DecodeBits(iter, 32, value);
        break;
    default:
        ABSL_UNREACHABLE();
    }
}

size_t CalculateControlBlockSize(absl::Span<const uint64_t> in)
{
    size_t blockSizeBits = in.size() * 2 * 3;
    return (blockSizeBits + ValueBitsSize - 1) / ValueBitsSize;
}

size_t CalculateMaxExpectedSize(absl::Span<const uint64_t> in)
{
    return 1 + in.size() * 2 + CalculateControlBlockSize(in);
}

size_t Compress(const absl::Span<const uint64_t> in, uint32_t* out)
{
    out[0] = static_cast<uint32_t>(in.size());
    MutBitIterator iter{out};
    iter.AdvanceBytes(1);

    uint64_t prev = 0;
    uint64_t cur = 0;
    uint64_t diff = 0;

    for (size_t i = 0; i < in.size(); i++) {
        uint32_t isSigned = 0;
        cur = in[i];
        if (cur < prev) {
            isSigned = 1;
            std::swap(cur, prev);
        }

        diff = cur - prev;

        uint32_t high = static_cast<uint32_t>(diff >> 32);
        uint32_t low = static_cast<uint32_t>(diff);

        EncodeBits(iter, 1, isSigned);
        Encode(iter, low);
        Encode(iter, high);

        prev = in[i];
    }
    return iter.valuePos;
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
    size_t total = in[0];
    out.clear();
    out.resize(total);
    Decompress(in, out.data());
}

size_t Decompress(absl::Span<const uint32_t> in, uint64_t* out)
{
    size_t total = in[0];
    ConstBitIterator iter{in};
    iter.AdvanceBytes(1);

    uint64_t prev = 0;
    uint64_t cur = 0;
    uint64_t diff = 0;

    uint32_t high = 0;
    uint32_t low = 0;
    uint32_t isSigned = 0;

    for (size_t i = 0; i < total; i++) {
        DecodeBits(iter, 1, isSigned);

        Decode(iter, low);
        Decode(iter, high);

        diff = (static_cast<uint64_t>(high) << 32) | low;

        if (isSigned) {
            cur = prev - diff;
        } else {
            cur = prev + diff;
        }

        out[i] = cur;
        prev = cur;
    }
    return total;
}

} // namespace bit_packing
} // namespace memhawk
