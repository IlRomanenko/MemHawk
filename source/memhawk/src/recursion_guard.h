#pragma once

#include <cstdint>

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

    static inline thread_local uint32_t level = 0;
};

struct AllocTag
{
};

struct InnerAllocTag
{
};
