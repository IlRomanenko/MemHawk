#pragma once

#include <absl/types/span.h>

#include <cstddef>

namespace memhawk
{

constexpr const size_t MaxCollapseDepth = 8;

size_t CollapseRecursionNaive(absl::Span<void*> data, size_t depth);
size_t CollapseRecursion(absl::Span<void*> data, size_t depth);

} // namespace memhawk
