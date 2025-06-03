#pragma once

#include <cstdint>
#include <emmintrin.h>

namespace memhawk
{

struct AllocInfo
{
    uint64_t size{};
    uint32_t offset{}; // in case of alignment
    uint32_t traceId{};

    explicit AllocInfo(uint64_t ctrSize, uint32_t ctrOffset) : size(ctrSize), offset(ctrOffset), traceId(0)
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

    AllocSummary& operator+=(const AllocSummary& rhs) noexcept
    {
        size += rhs.size;
        overhead += rhs.overhead;
        active += rhs.active;
        totalBytes += rhs.totalBytes;
        totalCount += rhs.totalCount;
        return *this;
    }

    AllocSummary& operator-=(const AllocSummary& rhs) noexcept
    {
        size -= rhs.size;
        overhead -= rhs.overhead;
        active -= rhs.active;
        // sum of all allocations
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
