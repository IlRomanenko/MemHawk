#pragma once

#include "macros.h"

#include <cstddef>
#include <optional>

namespace memhawk
{

[[nodiscard]] ABSL_ATTRIBUTE_ALWAYS_INLINE inline std::optional<size_t> CheckedMult(size_t lhs, size_t rhs) noexcept
{
    size_t res{};
    if (unlikely(__builtin_mul_overflow(lhs, rhs, &res)))
    {
        return {};
    }
    return res;
}

[[nodiscard]] ABSL_ATTRIBUTE_ALWAYS_INLINE inline std::optional<size_t> CheckedAdd(size_t lhs, size_t rhs) noexcept
{
    size_t res{};
    if (unlikely(__builtin_add_overflow(lhs, rhs, &res)))
    {
        return {};
    }
    return res;
}

[[nodiscard]] ABSL_ATTRIBUTE_ALWAYS_INLINE inline std::optional<size_t> CheckedAdd(std::optional<size_t> lhs,
                                                                                   size_t rhs) noexcept
{
    if (unlikely(!lhs))
    {
        return {};
    }
    size_t res{};
    if (unlikely(__builtin_add_overflow(*lhs, rhs, &res)))
    {
        return {};
    }
    return res;
}

[[nodiscard]] ABSL_ATTRIBUTE_ALWAYS_INLINE inline std::optional<size_t> CheckedAlignUp(size_t value,
                                                                                       size_t alignment) noexcept
{
    if (!alignment)
    {
        return {};
    }
    auto reminder = value % alignment;
    if (!reminder)
    {
        return value;
    }
    return CheckedAdd(value - reminder, alignment);
}

} // namespace memhawk
