#pragma once

#include <absl/base/attributes.h>

#include <cstdint>

namespace memhawk
{

template <typename T>
struct RecursionGuard
{
    RecursionGuard()
    {
        level++;
    }

    ~RecursionGuard()
    {
        level--;
    }

    RecursionGuard(const RecursionGuard&) = delete;
    RecursionGuard(RecursionGuard&&) = delete;
    RecursionGuard& operator=(const RecursionGuard&) = delete;
    RecursionGuard& operator=(RecursionGuard&&) = delete;

    explicit operator bool() const
    {
        return IsFirst();
    }

    bool IsFirst() const
    {
        return level == 1;
    }

    uint32_t Level() const
    {
        return level;
    }

    ABSL_CONST_INIT static inline thread_local uint32_t level = 0;
};

struct AllocTag
{
};

struct InnerAllocTag
{
};

struct RetPtrTag
{
};

} // namespace memhawk
