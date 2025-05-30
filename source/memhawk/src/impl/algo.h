#pragma once

#include "config_defs.h"

#include <absl/types/span.h>

#include <cstddef>

namespace memhawk
{

constexpr const size_t MaxCollapseDepth = 8;

size_t CollapseRecursionNaive(absl::Span<void*> data, size_t depth);
size_t CollapseRecursion(absl::Span<void*> data, size_t depth);
size_t CollapseRecursionOpt(absl::Span<void*> data, size_t depth);

#ifdef COMPILER_SUPPORTS_AVX512
size_t CollapseRecursionAvx(absl::Span<void*> data, size_t depth);
#endif

} // namespace memhawk
