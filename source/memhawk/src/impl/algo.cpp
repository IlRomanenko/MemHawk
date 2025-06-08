#include "algo.h"

#include "config.h"
#include "macros.h"

#include <absl/base/attributes.h>
#include <absl/types/span.h>
#include <sys/cdefs.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef COMPILER_SUPPORTS_AVX512
#include <immintrin.h>
#endif

namespace memhawk
{

size_t CollapseRecursionNaive(absl::Span<void*> data, size_t depth)
{
    size_t left = 0;
    size_t cur = left + 1;
    size_t end = data.size();
    while (cur < end)
    {
        size_t cycleSize = 0;

        //  begin         left    cur    end
        // [  |    ....    |   ... | .... | ]
        for (size_t spanSize = 1; spanSize <= depth && cur + spanSize <= end; spanSize++)
        {
            bool matched = true;
            if (spanSize > left + 1)
            {
                break;
            }

#pragma unroll(2) // unroll few steps
            for (size_t p = 0; p < spanSize; p++)
            {
                if (data[cur + p] != data[left - spanSize + p + 1])
                {
                    matched = false;
                    break;
                }
            }
            if (matched)
            {
                cycleSize = spanSize;
                break;
            }
        }
        if (cycleSize > 0)
        {
            cur += cycleSize;
        }
        else
        {
            left++;
            data[left] = data[cur];
            cur++;
        }
    }
    end = left + 1;
    return end;
}

size_t CollapseRecursion(absl::Span<void*> data, size_t depth)
{
#ifdef COMPILER_SUPPORTS_AVX512
    return CollapseRecursionAvx(data, depth);
#else
    return CollapseRecursionOpt(data, depth);
#endif
}

size_t CollapseRecursionOpt(absl::Span<void*> data, size_t depth)
{
    if (depth == 0 || data.size() < 2)
    {
        return data.size();
    }
    uint8_t maxDp[MaxUnwindDepth]{};

    uint32_t cur = 1;
    uint32_t end = data.size();

    const void* prevValue = nullptr;
    const void* nextValue = nullptr;
    uint8_t dp = 0;

    maxDp[0] = data[0] == data[1];

    for (uint32_t shift = 1; shift <= depth; shift++)
    {
        prevValue = data[0];
        dp = 0;
        cur = 1;
        uint32_t shifted = cur + shift - 1;

#pragma unroll(8) // unroll few steps
        while (shifted < end)
        {
            nextValue = data[shifted];
            if (likely(prevValue != nextValue))
            {
                dp = 0;
            }
            else
            {
                dp++;
                if (dp >= shift)
                {
                    maxDp[cur] = shift;
                }
            }
            prevValue = data[cur];
            cur++;
            shifted++;
        }
    }

    uint32_t left = 0;
#pragma unroll(4) // unroll few steps
    for (cur = 1; cur < end;)
    {
        if (likely(maxDp[cur] == 0))
        {
            left++;
            data[left] = data[cur];
            cur++;
            continue;
        }
        cur += maxDp[cur];
    }

    end = left + 1;
    return end;
}

#ifdef COMPILER_SUPPORTS_AVX512
size_t CollapseRecursionAvx(absl::Span<void*> data, size_t depth)
{
    const size_t size = data.size();
    if (depth == 0 || size < 2)
    {
        return size;
    }
    uint8_t maxDp[MaxUnwindDepth]{};

    for (uint32_t shift = 1; shift <= depth; ++shift)
    {
        const uint32_t maxIndex = size - shift;
        if (maxIndex == 0)
        {
            break;
        }
        uint8_t lastDp = 0;

        constexpr uint32_t simdStride = 8;
        uint32_t i = 0;
        for (; i + simdStride <= maxIndex; i += simdStride)
        {
            __m512i current = _mm512_loadu_epi64(reinterpret_cast<const void*>(data.data() + i));
            __m512i shifted_elements = _mm512_loadu_epi64(reinterpret_cast<const void*>(data.data() + i + shift));

            __mmask8 cmp_mask = _mm512_cmpeq_epi64_mask(current, shifted_elements);
            uint8_t mask = static_cast<uint8_t>(cmp_mask);

#pragma unroll(8)
            for (uint32_t j = 0; j < simdStride; ++j)
            {
                if (likely((mask & 1) == 0))
                {
                    lastDp = 0;
                }
                else
                {
                    lastDp++;
                    if (unlikely(lastDp >= shift))
                    {
                        maxDp[i + j + 1] = shift;
                    }
                }
                mask >>= 1;
            }
        }

// Handle remaining elements
#pragma unroll(4)
        for (; i < maxIndex; ++i)
        {
            if (likely(data[i] != data[i + shift]))
            {
                lastDp = 0;
            }
            else
            {
                lastDp++;
                if (lastDp >= shift)
                {
                    maxDp[i + 1] = shift;
                }
            }
        }
    }

    uint32_t left = 0;
#pragma unroll(4)
    for (uint32_t cur = 1; cur < size;)
    {
        if (maxDp[cur] == 0)
        {
            ++left;
            data[left] = data[cur];
            ++cur;
        }
        else
        {
            cur += maxDp[cur];
        }
    }
    return left + 1;
}
#endif

} // namespace memhawk
