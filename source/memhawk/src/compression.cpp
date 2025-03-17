#include "compression.h"

namespace memhawk
{

enum class SizeBits
{
    Zero = 0,
    TwoBytes = 1,
    ThreeBytes = 2,
    FourBytes = 3
};

enum class SignBit
{
    Unsigned = 0,
    Signed = 1
};

void Compress(absl::Span<uint64_t> in, std::vector<uint32_t>& out)
{
    out.clear();
    size_t neededBits = in.size() * 2 * 3;
    size_t neededBytes = (neededBits + 7) / 8; // ceil
    out.resize(neededBytes);

    for (size_t i = 1; i < in.size(); i++) {
    }
}

void Decompress(absl::Span<uint32_t> in, std::vector<uint64_t>& out)
{
    out.clear();
}

} // namespace memhawk
