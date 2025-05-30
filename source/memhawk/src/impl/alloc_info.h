#pragma once

#include <cstdint>
#include <emmintrin.h>
#include <utility>

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
    int64_t active{};
    int64_t overhead{};

    // total allocations
    uint64_t totalCount{};
    uint64_t totalBytes{};

    AllocSummary() = default;
    ~AllocSummary() = default;

    AllocSummary& operator+=(const AllocSummary& rhs) noexcept
    {
        size += rhs.size;
        active += rhs.active;
        overhead += rhs.overhead;
        totalBytes += rhs.totalBytes;
        totalCount += rhs.totalCount;
        return *this;
    }

    AllocSummary& operator-=(const AllocSummary& rhs) noexcept
    {
        size -= rhs.size;
        active -= rhs.active;
        overhead -= rhs.overhead;
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
        totalBytes += rhs.size;
        totalCount++;
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

struct TracedAllocSummary
{
    bool changed{false};
    uint32_t traceId{};
    AllocSummary summary;
    AllocSummary diff;

    explicit TracedAllocSummary(uint32_t ctrTraceId) : traceId(ctrTraceId)
    {
    }

    TracedAllocSummary& operator+=(const TracedAllocSummary& rhs) noexcept
    {
        changed = true;
        summary += rhs.summary;
        diff += rhs.diff;
        return *this;
    }

    TracedAllocSummary& operator-=(const TracedAllocSummary& rhs) noexcept
    {
        changed = true;
        summary -= rhs.summary;
        diff -= rhs.diff;
        return *this;
    }

    TracedAllocSummary& operator+=(const AllocInfo& rhs) noexcept
    {
        changed = true;
        summary += rhs;
        diff += rhs;
        return *this;
    }

    TracedAllocSummary& operator-=(const AllocInfo& rhs) noexcept
    {
        changed = true;
        summary -= rhs;
        diff -= rhs;
        return *this;
    }

    AllocSummary ConsumeDiff()
    {
        changed = false;
        AllocSummary tmp;
        std::swap(tmp, diff);
        return tmp;
    }
};

} // namespace memhawk
