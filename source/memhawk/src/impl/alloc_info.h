#pragma once

#include <cstdint>
#include <emmintrin.h>

namespace memhawk
{

struct AllocInfo
{
    // user requested size
    uint64_t size{};
    // offset from original pointer to user data segment
    uint32_t offset{};
    uint32_t traceId{};

    constexpr explicit AllocInfo(uint64_t ctrSize, uint32_t ctrOffset) : size(ctrSize), offset(ctrOffset), traceId(0)
    {
    }
};

struct AllocSummary
{
    // signed integers, because we can track deallocation of pointer,
    // that was allocated in another thread
    int64_t size{};
    int64_t overhead{};
    int64_t active{};

    // total allocations
    uint64_t totalCount{};
    uint64_t totalBytes{};

    AllocSummary() = default;
    ~AllocSummary() = default;

    constexpr AllocSummary(int64_t ctrSize, int64_t ctrOverhead, int64_t ctrActive, uint64_t ctrTotalCount,
                           uint64_t ctrTotalBytes)
        : size{ctrSize}, overhead{ctrOverhead}, active{ctrActive}, totalCount{ctrTotalCount}, totalBytes{ctrTotalBytes}
    {
    }

    AllocSummary& operator+=(const AllocSummary& rhs) noexcept
    {
        size += rhs.size;
        overhead += rhs.overhead;
        active += rhs.active;
        totalBytes += rhs.totalBytes;
        totalCount += rhs.totalCount;
        return *this;
    }

    AllocSummary& operator+=(const AllocInfo& rhs) noexcept
    {
        size += static_cast<int64_t>(rhs.size);
        overhead += static_cast<int64_t>(rhs.offset);
        active++;
        totalCount++;
        totalBytes += rhs.size;
        return *this;
    }

    AllocSummary& operator-=(const AllocInfo& rhs) noexcept
    {
        size -= static_cast<int64_t>(rhs.size);
        overhead -= static_cast<int64_t>(rhs.offset);
        active--;
        // don't change total
        return *this;
    }
};

} // namespace memhawk
