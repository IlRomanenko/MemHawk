#pragma once

#include <cstdint>
#include <emmintrin.h>
#include <utility>

struct AllocInfo
{
    uint64_t size{};
    uint32_t offset{}; // in case of alignment
    uint32_t traceHash{};
};

struct AllocSummary
{
    // signed integers, because we can track deallocation of pointer,
    // that was allocated in another thread
    int64_t size{};
    int64_t count{};
    int64_t overhead{};

    // total allocations
    uint64_t total{};

    inline AllocSummary& operator+=(const AllocSummary& rhs)
    {
        size += rhs.size;
        count += rhs.count;
        overhead += rhs.overhead;
        total += rhs.total;
        return *this;
    }

    inline AllocSummary& operator-=(const AllocSummary& rhs)
    {
        size -= rhs.size;
        count -= rhs.count;
        overhead -= rhs.overhead;
        total += rhs.total; // sum of allocations
        return *this;
    }

    inline AllocSummary& operator+=(const AllocInfo& rhs)
    {
        size += rhs.size;
        overhead += rhs.offset;
        count++;
        total++;
        return *this;
    }

    inline AllocSummary& operator-=(const AllocInfo& rhs)
    {
        size -= rhs.size;
        overhead -= rhs.offset;
        count--;
        // doesn't change total
        return *this;
    }
};

struct TracedAllocSummary
{
    bool changed{false};
    uint32_t traceHash{};
    AllocSummary summary{};
    AllocSummary diff{};

    explicit TracedAllocSummary(uint32_t ctrTrace) : traceHash(ctrTrace)
    {
    }

    TracedAllocSummary& operator+=(const TracedAllocSummary& rhs)
    {
        changed = true;
        summary += rhs.summary;
        diff += rhs.diff;
        return *this;
    }

    TracedAllocSummary& operator-=(const TracedAllocSummary& rhs)
    {
        changed = true;
        summary -= rhs.summary;
        diff -= rhs.diff;
        return *this;
    }

    TracedAllocSummary& operator+=(const AllocInfo& rhs)
    {
        changed = true;
        summary += rhs;
        diff += rhs;
        return *this;
    }

    TracedAllocSummary& operator-=(const AllocInfo& rhs)
    {
        changed = true;
        summary -= rhs;
        diff -= rhs;
        return *this;
    }

    AllocSummary ConsumeDiff()
    {
        changed = false;
        AllocSummary tmp{};
        std::swap(tmp, diff);
        return tmp;
    }

private:
    TracedAllocSummary(uint32_t trace, int64_t size, int64_t count)
        : traceHash(trace), summary{size, count}, diff{size, count}
    {
    }
};
