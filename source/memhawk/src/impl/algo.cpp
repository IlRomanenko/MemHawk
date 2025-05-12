#include "algo.h"

#include "config.h"
#include "macros.h"

#include <absl/base/attributes.h>
#include <absl/base/prefetch.h>
#include <absl/types/span.h>
#include <sys/cdefs.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

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
    if (depth == 0)
    {
        return data.size();
    }
    if (depth > MaxCollapseDepth)
    {
        depth = MaxCollapseDepth;
    }
    std::array<uint8_t, MaxCollapseDepth> dp{};

    uint32_t left = 0;
    uint32_t cur = left + 1;
    uint32_t end = data.size();

    uint8_t cycleSize = 0;

    // should be always data[cur - 1]
    const void* prevValue = data[left];

    while (cur < end)
    {
        for (uint32_t shift = 0; shift < depth && shift + cur < end; shift++)
        {
            const auto curValue = data[cur + shift];
            if (prevValue != curValue)
            {
                dp[shift] = 0;
            }
            else
            {
                dp[shift] += 1;
                //  l 01 -> got match , also check that cycle is not processing
                // ab ab, dp = 2, i = 1
                if (dp[shift] >= shift + 1)
                {
                    if (!cycleSize)
                    {
                        cycleSize = shift + 1;
                    }
                }
            }
        }
        // swap cur, prev
        prevValue = data[cur];

        if (!cycleSize)
        {
            left++;
            data[left] = data[cur];
        }
        else
        {
            cycleSize--;
        }
        cur++;
    }
    end = left + 1;
    return end;
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

} // namespace memhawk
