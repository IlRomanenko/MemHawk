#include "algo.h"

#include <absl/base/attributes.h>
#include <absl/types/span.h>
#include <sys/cdefs.h>

#include <cstddef>
#include <cstdint>

namespace memhawk
{

size_t CollapseRecursionNaive(absl::Span<void*> data, size_t depth)
{
    const size_t begin = 0;
    size_t left = 0;
    size_t cur = left + 1;
    size_t end = data.size();
    while (cur < end) {
        size_t cycleSize = 0;

        //  begin         left    cur    end
        // [  |    ....    |   ... | .... | ]
        for (size_t spanSize = 1; spanSize <= depth && cur + spanSize <= end; spanSize++) {
            bool matched = true;
            if (begin + spanSize > left + 1) {
                break;
            }
            for (size_t p = 0; p < spanSize; p++) {
                if (data[cur + p] != data[left - spanSize + p + 1]) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                cycleSize = spanSize;
                break;
            }
        }
        if (cycleSize > 0) {
            cur += cycleSize;
        } else {
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
    if (depth == 0) {
        return data.size();
    }
    if (depth > MaxCollapseDepth) {
        depth = MaxCollapseDepth;
    }
    std::array<uint32_t, MaxCollapseDepth> dp{};

    size_t left = 0;
    size_t cur = left + 1;
    size_t end = data.size();

    size_t cycleSize = 0;

    // should be always data[cur - 1]
    const void* prevValue = data[left];

    while (cur < end) {
        for (size_t i = 0; i < depth && i + cur < end; i++) {
            const auto curValue = data[cur + i];
            if (prevValue != curValue) {
                dp[i] = 0;
            } else {
                dp[i] += 1;
                //  l 01 -> got match , also check that cycle is not processing
                // ab ab, dp = 2, i = 1
                if (dp[i] == i + 1) {
                    if (!cycleSize) {
                        cycleSize = i + 1;
                    }
                    // first element will be evicted, decrease match length
                    dp[i] -= 1;
                }
            }
        }
        // swap cur, prev
        prevValue = data[cur];

        if (!cycleSize) {
            left++;
            data[left] = data[cur];
        } else {
            cycleSize--;
        }
        cur++;
    }
    end = left + 1;
    return end;
}

} // namespace memhawk
