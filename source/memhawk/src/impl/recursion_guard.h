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

    explicit operator bool()
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

} // namespace memhawk
